#include "lockdep.h"

/**
 * Detect an actual deadlock by following the runtime wait-for chain.
 * Caller must hold g_meta_lock.
 * @param self_slot The slot ID of the current thread.
 * @param target_lock The ID of the lock being waited on.
 * @param owner_out A pointer to store the owner slot ID if found.
 * @return 1 if an actual deadlock is detected, 0 otherwise.
 */
static int lockdep_detect_actual_deadlock_locked(int self_slot, unsigned int target_lock, int *owner_out) {
    int owner = g_locks[target_lock].owner_slot;
    if (owner_out) {
        *owner_out = owner;
    }

    while (owner != -1) {
        if (owner == self_slot) {
            return 1;
        }

        unsigned int next_lock = g_threads[owner].waiting_on;
        if (next_lock == (unsigned int) -1) {
            return 0;
        }

        owner = g_locks[next_lock].owner_slot;
    }

    return 0;
}

/**
 * Update waiting state and check for actual deadlock before blocking on a mutex.
 * @param mutex The mutex to lock.
 * @return 1 if a deadlock is detected, 0 otherwise.
 */
int lockdep_before_blocking_mutex_lock(pthread_mutex_t *mutex) {
    int self_slot = lockdep_get_or_register_thread_slot();
    unsigned int target_lock = lockdep_lookup_or_create_lock_id(mutex);
    int owner_slot = -1;
    int found_deadlock;

    lockdep_meta_lock();
    g_threads[self_slot].waiting_on = target_lock;
    found_deadlock = lockdep_detect_actual_deadlock_locked(self_slot, target_lock, &owner_slot);
    lockdep_meta_unlock();

    if (found_deadlock) {
        lockdep_report_actual_deadlock((unsigned int)self_slot,
                                       target_lock,
                                       owner_slot);
        return 1;
    }

    return 0;
}

/**
 * Clear waiting state.
 */
void lockdep_cancel_wait(void) {
    int self_slot = lockdep_get_or_register_thread_slot();

    lockdep_meta_lock();
    g_threads[self_slot].waiting_on = (unsigned int) -1;
    lockdep_meta_unlock();
}

/**
 * Acquire a mutex and update the lock dependency tracking.
 * @param mutex The mutex to acquire.
 * @param via_trylock Whether the mutex was acquired via trylock.
 */
void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock) {
    (void)via_trylock;

    // Get or register thread/lock
    int self_slot = lockdep_get_or_register_thread_slot();
    unsigned int id = lockdep_lookup_or_create_lock_id(mutex);

    // Log the old held-set before pushing the new lock
    lockdep_log_held_context(id);

    // Update graph
    for (unsigned int i = 0; i < tls_state.held_count; i++) {
        lockdep_add_edge_and_check_cycle(tls_state.held[i], id);
    }

    // Push new held lock
    lockdep_push_held(id);

    // Update ownership
    lockdep_meta_lock();
    g_threads[self_slot].waiting_on = (unsigned int) -1;
    g_locks[id].owner_slot = self_slot;
    lockdep_meta_unlock();
}

/**
 * Release a mutex and update the lock dependency tracking.
 * @param mutex The mutex to release.
 */
void lockdep_release_mutex(pthread_mutex_t *mutex) {
    int self_slot = lockdep_get_or_register_thread_slot();
    unsigned int id;
    if (!lockdep_lookup_lock_id(mutex, &id)) {
        dprintf(2, "[LOCKDEP] warning: unlock on unknown mutex=%p tid=%d\n",
                (void *)mutex, gettid());
        return;
    }

    // Clear ownership
    lockdep_meta_lock();
    if (g_locks[id].owner_slot == self_slot) {
        g_locks[id].owner_slot = -1;
    }
    g_threads[self_slot].waiting_on = (unsigned int) -1;
    lockdep_meta_unlock();

    lockdep_remove_held(id);
}
