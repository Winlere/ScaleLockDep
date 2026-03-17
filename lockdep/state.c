#include "lockdep.h"

/* Global metadata lock */
atomic_flag g_meta_lock = ATOMIC_FLAG_INIT;

/* Lock slot registry */
lockdep_lock_slot_t g_lock_slots[LOCKDEP_MAX_LOCK_SLOTS];
int g_lock_slot_count = 0;

/* Thread slot registry */
lockdep_thread_slot_t g_thread_slots[LOCKDEP_MAX_THREAD_SLOTS];
int g_thread_slot_count = 0;

/* Per-thread state */
__thread lockdep_thread_state_t tls_thread_state = {0};
__thread int tls_thread_slot = LOCKDEP_INVALID_SLOT;

/* Runtime config */
int g_debug_enabled = 0;

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
 * Look up the lock slot for a mutex address.
 * @param mutex The mutex to look up.
 * @param lock_slot_out Where to write the lock slot if found.
 * @return 1 if found, 0 otherwise.
 */
int lockdep_lookup_lock_slot(pthread_mutex_t *mutex, int *lock_slot_out) {
    lockdep_meta_lock();

    for (int lock_slot = 0; lock_slot < g_lock_slot_count; lock_slot++) {
        if (g_lock_slots[lock_slot].addr == mutex) {
            *lock_slot_out = lock_slot;
            lockdep_meta_unlock();
            return 1;
        }
    }

    lockdep_meta_unlock();
    return 0;
}

/**
 * Look up the lock slot for a mutex address, or create a new slot if not found.
 * @param mutex The mutex to look up or create.
 * @return The lock slot.
 */
int lockdep_lookup_or_create_lock_slot(pthread_mutex_t *mutex) {
    lockdep_meta_lock();

    for (int lock_slot = 0; lock_slot < g_lock_slot_count; lock_slot++) {
        if (g_lock_slots[lock_slot].addr == mutex) {
            lockdep_meta_unlock();
            return lock_slot;
        }
    }

    if (g_lock_slot_count >= LOCKDEP_MAX_LOCK_SLOTS) {
        lockdep_meta_unlock();
        lockdep_panic("[LOCKDEP] too many lock slots\n");
    }

    int new_lock_slot = g_lock_slot_count++;
    g_lock_slots[new_lock_slot].addr = mutex;
    g_lock_slots[new_lock_slot].owner_thread_slot = LOCKDEP_INVALID_SLOT;

    lockdep_meta_unlock();
    return new_lock_slot;
}

/**
 * Push a lock slot onto the current thread's held-lock stack.
 * @param lock_slot The lock slot to push.
 */
void lockdep_push_held_lock_slot(int lock_slot) {
    if (tls_thread_state.held_lock_slot_count >= LOCKDEP_MAX_HELD_LOCK_SLOTS) {
        lockdep_panic("[LOCKDEP] held-lock stack overflow\n");
    }

    tls_thread_state.held_lock_slots[tls_thread_state.held_lock_slot_count++] = lock_slot;
}

/**
 * Remove a lock slot from the current thread's held-lock stack.
 * @param lock_slot The lock slot to remove.
 */
void lockdep_remove_held_lock_slot(int lock_slot) {
    for (int i = tls_thread_state.held_lock_slot_count - 1; i >= 0; i--) {
        if (tls_thread_state.held_lock_slots[i] == lock_slot) {
            for (int j = i; j + 1 < tls_thread_state.held_lock_slot_count; j++) {
                tls_thread_state.held_lock_slots[j] = tls_thread_state.held_lock_slots[j + 1];
            }
            tls_thread_state.held_lock_slot_count--;
            return;
        }
    }

    dprintf(2,
            "[LOCKDEP] warning: unlock unknown-held lock slot L%d (tid=%d)\n",
            lock_slot,
            gettid());
}

/**
 * Get the current thread slot, registering the thread if necessary.
 * @return The thread slot.
 */
int lockdep_get_or_register_thread_slot(void) {
    if (tls_thread_slot != LOCKDEP_INVALID_SLOT) {
        return tls_thread_slot;
    }

    pid_t tid = gettid();
    lockdep_meta_lock();

    for (int thread_slot = 0; thread_slot < g_thread_slot_count; thread_slot++) {
        if (g_thread_slots[thread_slot].tid == tid) {
            tls_thread_slot = thread_slot;
            lockdep_meta_unlock();
            return tls_thread_slot;
        }
    }

    if (g_thread_slot_count >= LOCKDEP_MAX_THREAD_SLOTS) {
        lockdep_meta_unlock();
        lockdep_panic("[LOCKDEP] too many thread slots\n");
    }

    int new_thread_slot = g_thread_slot_count++;
    g_thread_slots[new_thread_slot].tid = tid;
    g_thread_slots[new_thread_slot].waiting_on_lock_slot = LOCKDEP_INVALID_SLOT;
    tls_thread_slot = new_thread_slot;

    lockdep_meta_unlock();
    return tls_thread_slot;
}
