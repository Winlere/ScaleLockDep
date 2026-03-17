#include "lockdep.h"

/**
 * Collect the actual wait-for chain starting from
 * Tself -> Ltarget -> Towner -> ...
 * Caller must hold g_meta_lock.
 *
 * thread_chain[0] is the current thread slot.
 * For each edge i:
 *   thread_chain[i] -> lock_chain[i] -> thread_chain[i + 1]
 *
 * @return 1 if the chain closes back to the current thread, 0 otherwise.
 */
static int lockdep_collect_actual_deadlock_chain_locked(int self_thread_slot,
                                                        int target_lock_slot,
                                                        int *thread_chain,
                                                        int *lock_chain,
                                                        int *edge_count_out) {
    int edge_count = 0;
    int current_lock_slot = target_lock_slot;

    thread_chain[0] = self_thread_slot;

    while (edge_count < LOCKDEP_MAX_THREAD_SLOTS) {
        int owner_thread_slot = g_lock_slots[current_lock_slot].owner_thread_slot;
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

        current_lock_slot = g_thread_slots[owner_thread_slot].waiting_on_lock_slot;
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
int lockdep_before_blocking_mutex_lock(pthread_mutex_t *mutex) {
    int self_thread_slot = lockdep_get_or_register_thread_slot();
    int target_lock_slot = lockdep_lookup_or_create_lock_slot(mutex);
    int thread_chain[LOCKDEP_MAX_THREAD_SLOTS + 1];
    int lock_chain[LOCKDEP_MAX_THREAD_SLOTS];
    int edge_count = 0;
    int found_deadlock = 0;

    lockdep_meta_lock();
    g_thread_slots[self_thread_slot].waiting_on_lock_slot = target_lock_slot;
    found_deadlock = lockdep_collect_actual_deadlock_chain_locked(self_thread_slot,
                                                                  target_lock_slot,
                                                                  thread_chain,
                                                                  lock_chain,
                                                                  &edge_count);
    lockdep_meta_unlock();

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
    int self_thread_slot = lockdep_get_or_register_thread_slot();

    lockdep_meta_lock();
    g_thread_slots[self_thread_slot].waiting_on_lock_slot = LOCKDEP_INVALID_SLOT;
    lockdep_meta_unlock();
}

/**
 * Acquire a mutex and update lockdep state.
 * Potential deadlock tracking uses the global dependency graph.
 * Actual deadlock tracking uses owner/waiting state.
 */
void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock) {
    (void)via_trylock;

    int self_thread_slot = lockdep_get_or_register_thread_slot();
    int new_lock_slot = lockdep_lookup_or_create_lock_slot(mutex);

    /* Show the held set before pushing the new lock slot. */
    lockdep_log_held_lock_slots(new_lock_slot);

    /* For each currently held lock slot H, record dependency H -> new_lock_slot. */
    for (int i = 0; i < tls_thread_state.held_lock_slot_count; i++) {
        lockdep_add_edge_and_check_cycle(tls_thread_state.held_lock_slots[i],
                                         new_lock_slot);
    }

    /* Update the current thread's held-lock stack. */
    lockdep_push_held_lock_slot(new_lock_slot);

    /* Update runtime ownership / waiting state. */
    lockdep_meta_lock();
    g_thread_slots[self_thread_slot].waiting_on_lock_slot = LOCKDEP_INVALID_SLOT;
    g_lock_slots[new_lock_slot].owner_thread_slot = self_thread_slot;
    lockdep_meta_unlock();
}

/**
 * Release a mutex and update lockdep state.
 */
void lockdep_release_mutex(pthread_mutex_t *mutex) {
    int self_thread_slot = lockdep_get_or_register_thread_slot();
    int lock_slot;

    if (!lockdep_lookup_lock_slot(mutex, &lock_slot)) {
        dprintf(2,
                "[LOCKDEP] warning: unlock unknown mutex=%p tid=%d\n",
                (void *)mutex,
                gettid());
        return;
    }

    lockdep_meta_lock();
    if (g_lock_slots[lock_slot].owner_thread_slot == self_thread_slot) {
        g_lock_slots[lock_slot].owner_thread_slot = LOCKDEP_INVALID_SLOT;
    }
    g_thread_slots[self_thread_slot].waiting_on_lock_slot = LOCKDEP_INVALID_SLOT;
    lockdep_meta_unlock();

    lockdep_remove_held_lock_slot(lock_slot);
}
