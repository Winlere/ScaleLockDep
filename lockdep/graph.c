#include "lockdep.h"

/*
 * Global dependency graph.
 * g_dependency_graph[u][v] == 1 means an observed dependency edge Lu -> Lv.
 */
static uint8_t g_dependency_graph[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_MAX_LOCK_SLOTS] = {{0}};
static lockdep_edge_info_t
    g_dependency_edge_info[LOCKDEP_MAX_LOCK_SLOTS][LOCKDEP_MAX_LOCK_SLOTS];
static _Atomic uint64_t
    g_dependency_predecessor_bits[LOCKDEP_MAX_LOCK_SLOTS][(LOCKDEP_MAX_LOCK_SLOTS + 63) / 64];

/* Optional small stats for debugging / future summary. */
static int g_dependency_edge_count = 0;
static int g_dependency_cycle_count = 0;

static inline int lockdep_global_bit_test(_Atomic uint64_t *bits, int lock_slot) {
    uint64_t word = atomic_load_explicit(&bits[lock_slot / 64], memory_order_acquire);
    return (word & (1ULL << (lock_slot % 64))) != 0;
}

int lockdep_global_has_unknown_predecessor(const lockdep_thread_state_t *thread_state,
                                           int new_lock_slot) {
    for (int i = 0; i < thread_state->held_lock_slot_count; i++) {
        int held_lock_slot = thread_state->held_lock_slots[i];

        if (held_lock_slot == new_lock_slot) {
            continue;
        }

        if (!lockdep_global_bit_test(g_dependency_predecessor_bits[new_lock_slot],
                                     held_lock_slot)) {
            return 1;
        }
    }

    return 0;
}

/**
 * DFS helper to find whether target_lock_slot is reachable from cur_lock_slot.
 * Also records parent pointers so we can reconstruct the existing dependency chain.
 * Caller must already hold g_meta_lock.
 */
static int lockdep_find_path_locked(int cur_lock_slot,
                                    int target_lock_slot,
                                    int visited[LOCKDEP_MAX_LOCK_SLOTS],
                                    int parent[LOCKDEP_MAX_LOCK_SLOTS]) {
    int lock_slot_count = atomic_load_explicit(&g_lock_slot_count, memory_order_acquire);

    if (cur_lock_slot == target_lock_slot) {
        return 1;
    }

    visited[cur_lock_slot] = 1;

    for (int next_lock_slot = 0; next_lock_slot < lock_slot_count; next_lock_slot++) {
        if (g_dependency_graph[cur_lock_slot][next_lock_slot] && !visited[next_lock_slot]) {
            parent[next_lock_slot] = cur_lock_slot;
            if (lockdep_find_path_locked(next_lock_slot,
                                         target_lock_slot,
                                         visited,
                                         parent)) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Add a dependency edge from one held lock slot to a newly acquired lock slot.
 * If the new edge closes a cycle, report a potential deadlock.
 */
void lockdep_add_edge_and_check_cycle(int from_lock_slot,
                                      int to_lock_slot,
                                      int thread_slot,
                                      uintptr_t acquire_pc) {
    if (from_lock_slot == to_lock_slot) {
        return;
    }

    int found_cycle = 0;
    int existing_chain[LOCKDEP_MAX_LOCK_SLOTS];
    lockdep_edge_info_t existing_edge_infos[LOCKDEP_MAX_LOCK_SLOTS];
    lockdep_edge_info_t new_edge_info = {
        .thread_slot = thread_slot,
        .acquire_pc = acquire_pc,
    };
    int existing_chain_len = 0;

    lockdep_meta_lock();

    if (!g_dependency_graph[from_lock_slot][to_lock_slot]) {
        int visited[LOCKDEP_MAX_LOCK_SLOTS] = {0};
        int parent[LOCKDEP_MAX_LOCK_SLOTS];
        for (int i = 0; i < LOCKDEP_MAX_LOCK_SLOTS; i++) {
            parent[i] = LOCKDEP_INVALID_SLOT;
        }

        /*
         * If Lto can already reach Lfrom, then adding Lfrom -> Lto closes a cycle.
         */
        if (lockdep_find_path_locked(to_lock_slot,
                                     from_lock_slot,
                                     visited,
                                     parent)) {
            int reverse_chain[LOCKDEP_MAX_LOCK_SLOTS];
            int reverse_chain_len = 0;
            int cur_lock_slot = from_lock_slot;

            found_cycle = 1;
            g_dependency_cycle_count++;

            while (cur_lock_slot != LOCKDEP_INVALID_SLOT &&
                   reverse_chain_len < LOCKDEP_MAX_LOCK_SLOTS) {
                reverse_chain[reverse_chain_len++] = cur_lock_slot;
                if (cur_lock_slot == to_lock_slot) {
                    break;
                }
                cur_lock_slot = parent[cur_lock_slot];
            }

            if (reverse_chain_len > 0 &&
                reverse_chain[reverse_chain_len - 1] == to_lock_slot) {
                for (int i = reverse_chain_len - 1; i >= 0; i--) {
                    existing_chain[existing_chain_len++] = reverse_chain[i];
                }
                for (int i = 0; i + 1 < existing_chain_len; i++) {
                    existing_edge_infos[i] =
                        g_dependency_edge_info[existing_chain[i]][existing_chain[i + 1]];
                }
            }
        }

        g_dependency_graph[from_lock_slot][to_lock_slot] = 1;
        g_dependency_edge_info[from_lock_slot][to_lock_slot] = new_edge_info;
        atomic_fetch_or_explicit(&g_dependency_predecessor_bits[to_lock_slot][from_lock_slot / 64],
                                 1ULL << (from_lock_slot % 64),
                                 memory_order_release);
        g_dependency_edge_count++;
    }

    lockdep_meta_unlock();

    if (found_cycle) {
        lockdep_report_potential_deadlock(from_lock_slot,
                                          to_lock_slot,
                                          existing_chain,
                                          existing_chain_len,
                                          &new_edge_info,
                                          existing_edge_infos);
    }
}
