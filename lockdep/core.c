#include "lockdep.h"

/**
 * Acquire a mutex and update the lock dependency tracking.
 * @param mutex The mutex to acquire.
 * @param via_trylock Whether the mutex was acquired via trylock.
 */
void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock) {
    (void)via_trylock;

    unsigned int id = lockdep_lookup_or_create_lock_id(mutex);

    // Log the old held-set before pushing the new lock
    lockdep_log_held_context(id);

    // Update graph
    for (unsigned int i = 0; i < tls_state.held_count; i++) {
        lockdep_add_edge_and_check_cycle(tls_state.held[i], id);
    }

    // Push new held lock
    lockdep_push_held(id);
}

/**
 * Release a mutex and update the lock dependency tracking.
 * @param mutex The mutex to release.
 */
void lockdep_release_mutex(pthread_mutex_t *mutex) {
    unsigned int id;
    if (!lockdep_lookup_lock_id(mutex, &id)) {
        dprintf(2, "[LOCKDEP] warning: unlock on unknown mutex=%p tid=%d\n",
                (void *)mutex, gettid());
        return;
    }

    lockdep_remove_held(id);
}
