#include "lockdep.h"

static int lockdep_try_lookup_top_held_lock_slot(pthread_mutex_t *mutex, int *lock_slot_out) {
    int held_lock_slot_count = tls_thread_state.held_lock_slot_count;

    if (held_lock_slot_count == 0) {
        return 0;
    }

    int top_lock_slot = tls_thread_state.held_lock_slots[held_lock_slot_count - 1];

    if (g_lock_slots[top_lock_slot].addr != mutex) {
        return 0;
    }

    *lock_slot_out = top_lock_slot;
    return 1;
}

/**
 * Collect the actual wait-for chain starting from
 * Tself -> Ltarget -> Towner -> ...
 * thread_chain[0] is the current thread slot.
 * For each edge i:
 *   thread_chain[i] -> lock_chain[i] -> thread_chain[i + 1]
 *
 * @return 1 if the chain closes back to the current thread, 0 otherwise.
 */
static int lockdep_collect_actual_deadlock_chain(int self_thread_slot,
                                                 int target_lock_slot,
                                                 int *thread_chain,
                                                 int *lock_chain,
                                                 int *edge_count_out) {
    int edge_count = 0;
    int current_lock_slot = target_lock_slot;

    thread_chain[0] = self_thread_slot;

    while (edge_count < LOCKDEP_MAX_THREAD_SLOTS) {
        int owner_thread_slot = atomic_load_explicit(
            &g_lock_slots[current_lock_slot].owner_thread_slot,
            memory_order_acquire);
        if (owner_thread_slot == LOCKDEP_INVALID_SLOT) {
            *edge_count_out = edge_count;
            return 0;
        }

        lock_chain[edge_count] = current_lock_slot;
        thread_chain[edge_count + 1] = owner_thread_slot;
        edge_count++;

        if (owner_thread_slot == self_thread_slot) {
            *edge_count_out = edge_count;
            return 1;
        }

        current_lock_slot = atomic_load_explicit(
            &g_thread_slots[owner_thread_slot].waiting_on_lock_slot,
            memory_order_acquire);
        if (current_lock_slot == LOCKDEP_INVALID_SLOT) {
            *edge_count_out = edge_count;
            return 0;
        }
    }

    *edge_count_out = edge_count;
    return 0;
}

/**
 * Update waiting state and check for actual deadlock before blocking on a mutex.
 * @param mutex The mutex that will block the current thread.
 * @return 1 if an actual deadlock is detected, 0 otherwise.
 */
int lockdep_before_blocking_mutex_lock(pthread_mutex_t *mutex, uintptr_t waiting_pc) {
    int self_thread_slot = lockdep_current_thread_slot();
    int target_lock_slot = lockdep_lookup_or_create_lock_slot_cached_fast(mutex);
    int thread_chain[LOCKDEP_MAX_THREAD_SLOTS + 1];
    int lock_chain[LOCKDEP_MAX_THREAD_SLOTS];
    int edge_count = 0;
    int found_deadlock;

    atomic_store_explicit(&g_thread_slots[self_thread_slot].waiting_on_lock_slot,
                          target_lock_slot,
                          memory_order_release);
    atomic_store_explicit(&g_thread_slots[self_thread_slot].waiting_on_pc,
                          waiting_pc,
                          memory_order_release);
    found_deadlock = lockdep_collect_actual_deadlock_chain(self_thread_slot,
                                                           target_lock_slot,
                                                           thread_chain,
                                                           lock_chain,
                                                           &edge_count);

    if (found_deadlock) {
        lockdep_report_actual_deadlock(thread_chain, lock_chain, edge_count);
        return 1;
    }

    return 0;
}

/**
 * Clear waiting state for the current thread.
 */
void lockdep_cancel_wait(void) {
    int self_thread_slot = lockdep_current_thread_slot();

    atomic_store_explicit(&g_thread_slots[self_thread_slot].waiting_on_lock_slot,
                          LOCKDEP_INVALID_SLOT,
                          memory_order_release);
    atomic_store_explicit(&g_thread_slots[self_thread_slot].waiting_on_pc,
                          (uintptr_t)0,
                          memory_order_release);
}

/**
 * Acquire a mutex and update lockdep state.
 * Potential deadlock tracking uses the global dependency graph.
 * Actual deadlock tracking uses owner/waiting state.
 */
void lockdep_acquire_mutex(pthread_mutex_t *mutex,
                           int clear_wait_state,
                           uintptr_t acquire_pc) {
    int self_thread_slot = lockdep_current_thread_slot();
    int new_lock_slot = lockdep_lookup_or_create_lock_slot_cached_fast(mutex);
    int held_lock_slot_count = tls_thread_state.held_lock_slot_count;

    if (held_lock_slot_count == 0) {
        tls_thread_state.held_lock_slots[0] = new_lock_slot;
        tls_thread_state.held_lock_slot_count = 1;
        lockdep_note_thread_holds_lock_slot(new_lock_slot);
    } else {
        lockdep_debug_log_held_lock_slots(new_lock_slot);
        lockdep_potential_on_acquire(self_thread_slot,
                                     new_lock_slot,
                                     &tls_thread_state,
                                     acquire_pc);
        lockdep_push_held_lock_slot(new_lock_slot);
    }
    if (clear_wait_state) {
        atomic_store_explicit(&g_thread_slots[self_thread_slot].waiting_on_lock_slot,
                              LOCKDEP_INVALID_SLOT,
                              memory_order_release);
        atomic_store_explicit(&g_thread_slots[self_thread_slot].waiting_on_pc,
                              (uintptr_t)0,
                              memory_order_release);
    }
    atomic_store_explicit(&g_lock_slots[new_lock_slot].owner_acquire_pc,
                          acquire_pc,
                          memory_order_release);
    atomic_store_explicit(&g_lock_slots[new_lock_slot].owner_thread_slot,
                          self_thread_slot,
                          memory_order_release);
}

/**
 * Release a mutex and update lockdep state.
 */
void lockdep_release_mutex(pthread_mutex_t *mutex) {
    int self_thread_slot = lockdep_current_thread_slot();
    int lock_slot;
    int held_lock_slot_count = tls_thread_state.held_lock_slot_count;

    if (!lockdep_try_lookup_top_held_lock_slot(mutex, &lock_slot) &&
        !lockdep_lookup_lock_slot_cached_fast(mutex, &lock_slot)) {
        dprintf(2,
                "[LOCKDEP] warning: unlock unknown mutex=%p tid=%d\n",
                (void *)mutex,
                gettid());
        return;
    }

    int expected_owner_thread_slot = self_thread_slot;
    if (atomic_compare_exchange_strong_explicit(&g_lock_slots[lock_slot].owner_thread_slot,
                                                &expected_owner_thread_slot,
                                                LOCKDEP_INVALID_SLOT,
                                                memory_order_acq_rel,
                                                memory_order_acquire)) {
        atomic_store_explicit(&g_lock_slots[lock_slot].owner_acquire_pc,
                              (uintptr_t)0,
                              memory_order_release);
    }

    if (held_lock_slot_count == 1 && tls_thread_state.held_lock_slots[0] == lock_slot) {
        lockdep_note_thread_releases_lock_slot(lock_slot);
        tls_thread_state.held_lock_slot_count = 0;
        return;
    }

    lockdep_remove_held_lock_slot(lock_slot);
}
