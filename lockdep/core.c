#include "lockdep.h"

/**
 * Acquire a mutex and update the lock dependency tracking.
 * @param mutex The mutex to acquire.
 * @param via_trylock Whether the mutex was acquired via trylock.
 */
void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock) {
    (void)via_trylock;

    unsigned int id = lockdep_lookup_or_create_lock_id(mutex);
    lockdep_log_held_context(id);
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
