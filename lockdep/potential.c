#include "lockdep.h"

#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    LOCKDEP_RB_FULL_STALL = 0,
    LOCKDEP_RB_FULL_DROP = 1,
} lockdep_rb_full_policy_t;

typedef struct {
    int to_lock_slot;
    uint64_t from_bits[LOCKDEP_GRAPH_WORDS];
} lockdep_potential_event_t;

typedef struct {
    _Atomic unsigned int head;
    _Atomic unsigned int tail;
    lockdep_potential_event_t events[LOCKDEP_RB_CAPACITY];
} lockdep_potential_ring_t;

static lockdep_potential_ring_t g_potential_rings[LOCKDEP_MAX_THREAD_SLOTS];
static uint64_t g_rb_adjacency_bits[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_GRAPH_WORDS];
static uint64_t g_rb_predecessor_bits[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_GRAPH_WORDS];
static uint64_t g_rb_reachability_bits[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_GRAPH_WORDS];
static _Atomic uint64_t
    g_rb_predecessor_snapshot[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_GRAPH_WORDS];
static pthread_t g_rb_worker;
static sem_t g_rb_wakeup_sem;
static _Atomic int g_rb_worker_should_stop = 0;
static _Atomic int g_rb_work_pending = 0;
static _Atomic int g_rb_worker_state = 0;
static lockdep_rb_full_policy_t g_rb_full_policy = LOCKDEP_RB_FULL_STALL;
static _Atomic unsigned long g_rb_dropped_events = 0;

lockdep_mode_t g_lockdep_mode = LOCKDEP_MODE_GLOBAL;

static void *lockdep_rb_worker_main(void *arg);

static inline int lockdep_graph_word_index(int lock_slot) {
    return lock_slot / 64;
}

static inline uint64_t lockdep_graph_bit_mask(int lock_slot) {
    return 1ULL << (lock_slot % 64);
}

static inline int lockdep_graph_bit_test(const uint64_t *bits, int lock_slot) {
    return (bits[lockdep_graph_word_index(lock_slot)] &
            lockdep_graph_bit_mask(lock_slot)) != 0;
}

static inline void lockdep_graph_bit_set(uint64_t *bits, int lock_slot) {
    bits[lockdep_graph_word_index(lock_slot)] |= lockdep_graph_bit_mask(lock_slot);
}

static int lockdep_graph_has_any_bits(const uint64_t *bits) {
    for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
        if (bits[word] != 0) {
            return 1;
        }
    }

    return 0;
}

static void lockdep_global_on_acquire(int new_lock_slot,
                                      const lockdep_thread_state_t *thread_state) {
    for (int i = 0; i < thread_state->held_lock_slot_count; i++) {
        lockdep_add_edge_and_check_cycle(thread_state->held_lock_slots[i], new_lock_slot);
    }
}

static int lockdep_rb_collect_unknown_predecessors(
    const lockdep_thread_state_t *thread_state,
    int new_lock_slot,
    uint64_t unknown_predecessors[LOCKDEP_GRAPH_WORDS]) {
    for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
        unknown_predecessors[word] = 0;
    }

    for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
        uint64_t known_predecessors = atomic_load_explicit(
            &g_rb_predecessor_snapshot[new_lock_slot][word],
            memory_order_acquire);
        unknown_predecessors[word] =
            thread_state->held_lock_bits[word] & ~known_predecessors;
    }
    unknown_predecessors[lockdep_graph_word_index(new_lock_slot)] &=
        ~lockdep_graph_bit_mask(new_lock_slot);

    return lockdep_graph_has_any_bits(unknown_predecessors);
}

static void lockdep_rb_ensure_worker_started(void) {
    int state = atomic_load_explicit(&g_rb_worker_state, memory_order_acquire);

    if (state == 2) {
        return;
    }

    state = 0;
    if (atomic_compare_exchange_strong_explicit(&g_rb_worker_state,
                                                &state,
                                                1,
                                                memory_order_acq_rel,
                                                memory_order_acquire)) {
        if (sem_init(&g_rb_wakeup_sem, 0, 0) != 0) {
            atomic_store_explicit(&g_rb_worker_state, 0, memory_order_release);
            lockdep_panic("[LOCKDEP] failed to initialize rb backend wakeup semaphore\n");
        }

        atomic_store_explicit(&g_rb_worker_should_stop, 0, memory_order_release);
        atomic_store_explicit(&g_rb_work_pending, 0, memory_order_release);
        if (pthread_create(&g_rb_worker, NULL, lockdep_rb_worker_main, NULL) != 0) {
            sem_destroy(&g_rb_wakeup_sem);
            atomic_store_explicit(&g_rb_worker_state, 0, memory_order_release);
            lockdep_panic("[LOCKDEP] failed to start rb backend worker thread\n");
        }

        atomic_store_explicit(&g_rb_worker_state, 2, memory_order_release);
        return;
    }

    while (atomic_load_explicit(&g_rb_worker_state, memory_order_acquire) != 2) {
        sched_yield();
    }
}

const char *lockdep_mode_name(lockdep_mode_t mode) {
    switch (mode) {
    case LOCKDEP_MODE_GLOBAL:
        return "global";
    case LOCKDEP_MODE_RB:
        return "rb";
    default:
        return "unknown";
    }
}

void lockdep_set_mode_from_env(const char *mode_env) {
    if (!mode_env || strcmp(mode_env, "global") == 0) {
        g_lockdep_mode = LOCKDEP_MODE_GLOBAL;
        return;
    }

    if (strcmp(mode_env, "rb") == 0) {
        g_lockdep_mode = LOCKDEP_MODE_RB;
        return;
    }

    lockdep_panic("[LOCKDEP] invalid LOCKDEP_MODE (expected 'global' or 'rb')\n");
}

static void lockdep_rb_signal_worker(void) {
    if (atomic_exchange_explicit(&g_rb_work_pending, 1, memory_order_acq_rel) == 0) {
        if (sem_post(&g_rb_wakeup_sem) != 0) {
            lockdep_panic("[LOCKDEP] failed to wake rb backend worker\n");
        }
    }
}

static void lockdep_rb_set_full_policy_from_env(const char *policy_env) {
    if (!policy_env || strcmp(policy_env, "stall") == 0) {
        g_rb_full_policy = LOCKDEP_RB_FULL_STALL;
        return;
    }

    if (strcmp(policy_env, "drop") == 0) {
        g_rb_full_policy = LOCKDEP_RB_FULL_DROP;
        return;
    }

    lockdep_panic("[LOCKDEP] invalid LOCKDEP_RB_FULL (expected 'stall' or 'drop')\n");
}

static void lockdep_rb_enqueue_edge_batch(int thread_slot,
                                          int to_lock_slot,
                                          const uint64_t from_bits[LOCKDEP_GRAPH_WORDS]) {
    lockdep_potential_ring_t *ring = &g_potential_rings[thread_slot];
    unsigned int tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);

    for (;;) {
        unsigned int head = atomic_load_explicit(&ring->head, memory_order_acquire);
        if (tail - head < LOCKDEP_RB_CAPACITY) {
            break;
        }
        if (g_rb_full_policy == LOCKDEP_RB_FULL_DROP) {
            atomic_fetch_add_explicit(&g_rb_dropped_events, 1, memory_order_relaxed);
            lockdep_rb_signal_worker();
            return;
        }
        lockdep_rb_signal_worker();
        sched_yield();
        tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    }

    lockdep_potential_event_t *event = &ring->events[tail % LOCKDEP_RB_CAPACITY];
    event->to_lock_slot = to_lock_slot;
    for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
        event->from_bits[word] = from_bits[word];
    }

    atomic_store_explicit(&ring->tail, tail + 1, memory_order_release);
    lockdep_rb_signal_worker();
}

static int lockdep_rb_try_dequeue_event(int thread_slot, lockdep_potential_event_t *event_out) {
    lockdep_potential_ring_t *ring = &g_potential_rings[thread_slot];
    unsigned int head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    unsigned int tail = atomic_load_explicit(&ring->tail, memory_order_acquire);

    if (head == tail) {
        return 0;
    }

    *event_out = ring->events[head % LOCKDEP_RB_CAPACITY];
    atomic_store_explicit(&ring->head, head + 1, memory_order_release);
    return 1;
}

static int lockdep_rb_find_path(int cur_lock_slot,
                                int target_lock_slot,
                                int visited[LOCKDEP_MAX_LOCK_SLOTS],
                                int parent[LOCKDEP_MAX_LOCK_SLOTS]) {
    int lock_slot_count = atomic_load_explicit(&g_lock_slot_count, memory_order_acquire);

    if (cur_lock_slot == target_lock_slot) {
        return 1;
    }

    visited[cur_lock_slot] = 1;

    for (int next_lock_slot = 0; next_lock_slot < lock_slot_count; next_lock_slot++) {
        if (lockdep_graph_bit_test(g_rb_adjacency_bits[cur_lock_slot], next_lock_slot) &&
            !visited[next_lock_slot]) {
            parent[next_lock_slot] = cur_lock_slot;
            if (lockdep_rb_find_path(next_lock_slot,
                                     target_lock_slot,
                                     visited,
                                     parent)) {
                return 1;
            }
        }
    }

    return 0;
}

static void lockdep_rb_report_cycle_if_any(int from_lock_slot, int to_lock_slot) {
    if (!lockdep_graph_bit_test(g_rb_reachability_bits[to_lock_slot], from_lock_slot)) {
        return;
    }

    int visited[LOCKDEP_MAX_LOCK_SLOTS] = {0};
    int parent[LOCKDEP_MAX_LOCK_SLOTS];
    int reverse_chain[LOCKDEP_MAX_LOCK_SLOTS];
    int existing_chain[LOCKDEP_MAX_LOCK_SLOTS];
    int reverse_chain_len = 0;
    int existing_chain_len = 0;
    int cur_lock_slot = from_lock_slot;

    for (int i = 0; i < LOCKDEP_MAX_LOCK_SLOTS; i++) {
        parent[i] = LOCKDEP_INVALID_SLOT;
    }

    if (!lockdep_rb_find_path(to_lock_slot, from_lock_slot, visited, parent)) {
        return;
    }

    while (cur_lock_slot != LOCKDEP_INVALID_SLOT &&
           reverse_chain_len < LOCKDEP_MAX_LOCK_SLOTS) {
        reverse_chain[reverse_chain_len++] = cur_lock_slot;
        if (cur_lock_slot == to_lock_slot) {
            break;
        }
        cur_lock_slot = parent[cur_lock_slot];
    }

    if (reverse_chain_len == 0 || reverse_chain[reverse_chain_len - 1] != to_lock_slot) {
        return;
    }

    for (int i = reverse_chain_len - 1; i >= 0; i--) {
        existing_chain[existing_chain_len++] = reverse_chain[i];
    }

    lockdep_report_potential_deadlock(from_lock_slot,
                                      to_lock_slot,
                                      existing_chain,
                                      existing_chain_len);
}

static void lockdep_rb_update_reachability(int from_lock_slot, int to_lock_slot) {
    uint64_t targets[LOCKDEP_GRAPH_WORDS];
    int lock_slot_count = atomic_load_explicit(&g_lock_slot_count, memory_order_acquire);

    for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
        targets[word] = g_rb_reachability_bits[to_lock_slot][word];
    }
    lockdep_graph_bit_set(targets, to_lock_slot);

    for (int src_lock_slot = 0; src_lock_slot < lock_slot_count; src_lock_slot++) {
        if (src_lock_slot != from_lock_slot &&
            !lockdep_graph_bit_test(g_rb_reachability_bits[src_lock_slot], from_lock_slot)) {
            continue;
        }

        for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
            g_rb_reachability_bits[src_lock_slot][word] |= targets[word];
        }
    }
}

static void lockdep_rb_add_edge_if_new(int from_lock_slot, int to_lock_slot) {
    if (from_lock_slot == to_lock_slot ||
        lockdep_graph_bit_test(g_rb_predecessor_bits[to_lock_slot], from_lock_slot)) {
        return;
    }

    lockdep_rb_report_cycle_if_any(from_lock_slot, to_lock_slot);
    lockdep_graph_bit_set(g_rb_adjacency_bits[from_lock_slot], to_lock_slot);
    lockdep_graph_bit_set(g_rb_predecessor_bits[to_lock_slot], from_lock_slot);
    atomic_fetch_or_explicit(
        &g_rb_predecessor_snapshot[to_lock_slot][lockdep_graph_word_index(from_lock_slot)],
        lockdep_graph_bit_mask(from_lock_slot),
        memory_order_release);
    lockdep_rb_update_reachability(from_lock_slot, to_lock_slot);
}

static void lockdep_rb_process_edge_bits(int to_lock_slot,
                                         const uint64_t from_bits[LOCKDEP_GRAPH_WORDS]) {
    int lock_slot_count = atomic_load_explicit(&g_lock_slot_count, memory_order_acquire);

    for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
        uint64_t bits = from_bits[word] & ~g_rb_predecessor_bits[to_lock_slot][word];

        while (bits != 0) {
            int bit = __builtin_ctzll(bits);
            int from_lock_slot = word * 64 + bit;

            if (from_lock_slot < lock_slot_count) {
                lockdep_rb_add_edge_if_new(from_lock_slot, to_lock_slot);
            }

            bits &= bits - 1;
        }
    }
}

static int lockdep_rb_drain_once(void) {
    int made_progress = 0;
    lockdep_potential_event_t event;
    uint64_t pending_from_bits[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_GRAPH_WORDS] = {{0}};
    unsigned char pending_touched[LOCKDEP_MAX_LOCK_SLOTS] = {0};
    int pending_to_locks[LOCKDEP_MAX_LOCK_SLOTS];
    int pending_to_lock_count = 0;

    for (int thread_slot = 0; thread_slot < LOCKDEP_MAX_THREAD_SLOTS; thread_slot++) {
        while (lockdep_rb_try_dequeue_event(thread_slot, &event)) {
            int to_lock_slot = event.to_lock_slot;

            made_progress = 1;

            if (to_lock_slot < 0 || to_lock_slot >= LOCKDEP_MAX_LOCK_SLOTS) {
                continue;
            }

            if (!pending_touched[to_lock_slot]) {
                pending_touched[to_lock_slot] = 1;
                pending_to_locks[pending_to_lock_count++] = to_lock_slot;
            }

            for (int word = 0; word < LOCKDEP_GRAPH_WORDS; word++) {
                pending_from_bits[to_lock_slot][word] |= event.from_bits[word];
            }
        }
    }

    for (int i = 0; i < pending_to_lock_count; i++) {
        int to_lock_slot = pending_to_locks[i];

        lockdep_rb_process_edge_bits(to_lock_slot, pending_from_bits[to_lock_slot]);
    }

    return made_progress;
}

static int lockdep_rb_all_queues_empty(void) {
    for (int thread_slot = 0; thread_slot < LOCKDEP_MAX_THREAD_SLOTS; thread_slot++) {
        lockdep_potential_ring_t *ring = &g_potential_rings[thread_slot];
        unsigned int head = atomic_load_explicit(&ring->head, memory_order_acquire);
        unsigned int tail = atomic_load_explicit(&ring->tail, memory_order_acquire);

        if (head != tail) {
            return 0;
        }
    }

    return 1;
}

static void *lockdep_rb_worker_main(void *arg) {
    (void)arg;

    lockdep_set_current_thread_internal(1);

    for (;;) {
        while (lockdep_rb_drain_once()) {
            /* keep draining while work is available */
        }

        atomic_store_explicit(&g_rb_work_pending, 0, memory_order_release);

        if (lockdep_rb_drain_once()) {
            atomic_store_explicit(&g_rb_work_pending, 1, memory_order_release);
            continue;
        }

        if (atomic_load_explicit(&g_rb_worker_should_stop, memory_order_acquire) &&
            lockdep_rb_all_queues_empty()) {
            break;
        }

        while (sem_wait(&g_rb_wakeup_sem) != 0) {
            if (errno != EINTR) {
                lockdep_panic("[LOCKDEP] rb backend worker wait failed\n");
            }
        }
    }

    return NULL;
}

void lockdep_potential_init(void) {
    if (g_lockdep_mode != LOCKDEP_MODE_RB) {
        return;
    }

    lockdep_rb_set_full_policy_from_env(getenv("LOCKDEP_RB_FULL"));
}

void lockdep_potential_shutdown(void) {
    if (atomic_load_explicit(&g_rb_worker_state, memory_order_acquire) != 2) {
        return;
    }

    atomic_store_explicit(&g_rb_worker_should_stop, 1, memory_order_release);
    if (sem_post(&g_rb_wakeup_sem) != 0) {
        lockdep_panic("[LOCKDEP] failed to wake rb backend worker during shutdown\n");
    }
    if (pthread_join(g_rb_worker, NULL) != 0) {
        lockdep_panic("[LOCKDEP] failed to join rb backend worker thread\n");
    }
    if (sem_destroy(&g_rb_wakeup_sem) != 0) {
        lockdep_panic("[LOCKDEP] failed to destroy rb backend wakeup semaphore\n");
    }

    atomic_store_explicit(&g_rb_worker_state, 0, memory_order_release);
}

void lockdep_potential_on_acquire(int thread_slot,
                                  int new_lock_slot,
                                  const lockdep_thread_state_t *thread_state) {
    uint64_t unknown_predecessors[LOCKDEP_GRAPH_WORDS];

    if (g_lockdep_mode == LOCKDEP_MODE_GLOBAL) {
        if (thread_state->held_lock_slot_count > 0 &&
            lockdep_global_has_unknown_predecessor(thread_state, new_lock_slot)) {
            lockdep_global_on_acquire(new_lock_slot, thread_state);
        }
        return;
    }

    if (thread_state->held_lock_slot_count == 0) {
        return;
    }

    if (!lockdep_rb_collect_unknown_predecessors(thread_state,
                                                new_lock_slot,
                                                unknown_predecessors)) {
        return;
    }

    lockdep_rb_ensure_worker_started();
    lockdep_rb_enqueue_edge_batch(thread_slot, new_lock_slot, unknown_predecessors);
}
