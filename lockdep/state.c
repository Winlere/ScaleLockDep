#include "lockdep.h"

// Global lock flag
atomic_flag g_meta_lock = ATOMIC_FLAG_INIT;

// Global lock registry
lockdep_lock_entry_t g_locks[LOCKDEP_MAX_LOCKS];

// Number of locks registered
int g_num_locks = 0;

// Per-thread state for held locks
__thread lockdep_thread_state_t tls_state = {0};

/**
 * Acquire the global metadata lock.
 */
void lockdep_meta_lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_meta_lock, memory_order_acquire)) {
        /* spin */
    }
}

/**
 * Release the global metadata lock.
 */
void lockdep_meta_unlock(void) {
    atomic_flag_clear_explicit(&g_meta_lock, memory_order_release);
}

/**
 * Look up the ID of a lock given its address.
 * @param mutex The mutex to look up.
 * @param out A pointer to store the lock ID if found.
 * @return 1 if the lock was found, 0 otherwise.
 */
int lockdep_lookup_lock_id(pthread_mutex_t *mutex, unsigned int *out) {
    lockdep_meta_lock();

    for (int i = 0; i < g_num_locks; i++) {
        if (g_locks[i].addr == mutex) {
            *out = g_locks[i].id;
            lockdep_meta_unlock();
            return 1;
        }
    }

    lockdep_meta_unlock();
    return 0;
}

/**
 * Look up the ID of a lock given its address, or create a new one if not found.
 * @param mutex The mutex to look up or create.
 * @return The ID of the lock.
 */
unsigned int lockdep_lookup_or_create_lock_id(pthread_mutex_t *mutex) {
    lockdep_meta_lock();

    for (int i = 0; i < g_num_locks; i++) {
        if (g_locks[i].addr == mutex) {
            unsigned int id = g_locks[i].id;
            lockdep_meta_unlock();
            return id;
        }
    }

    if (g_num_locks >= LOCKDEP_MAX_LOCKS) {
        lockdep_meta_unlock();
        lockdep_panic("[LOCKDEP] too many locks\n");
    }

    unsigned int new_id = (unsigned int)g_num_locks;
    g_locks[g_num_locks].addr = mutex;
    g_locks[g_num_locks].id = new_id;
    g_num_locks++;

    lockdep_meta_unlock();
    return new_id;
}

/**
 * Push a lock ID onto the stack of held locks for the current thread.
 * @param id The ID of the lock to push.
 */
void lockdep_push_held(unsigned int id) {
    if (tls_state.held_count >= LOCKDEP_MAX_HELD_LOCKS) {
        lockdep_panic("[LOCKDEP] held-lock stack overflow\n");
    }

    tls_state.held[tls_state.held_count++] = id;
}

/**
 * Remove a lock ID from the stack of held locks for the current thread.
 * @param id The ID of the lock to remove.
 */
void lockdep_remove_held(unsigned int id) {
    for (int i = (int)tls_state.held_count - 1; i >= 0; i--) {
        if (tls_state.held[i] == id) {
            for (int j = i; (unsigned int)(j + 1) < tls_state.held_count; j++) {
                tls_state.held[j] = tls_state.held[j + 1];
            }
            tls_state.held_count--;
            return;
        }
    }

    dprintf(2, "[LOCKDEP] warning: unlock unknown-held lock id=%u tid=%d\n",
            id, gettid());
}
