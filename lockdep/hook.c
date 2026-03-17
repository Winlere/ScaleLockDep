#include "lockdep.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>

/* Function pointers to the real pthread functions */
static int (*real_pthread_mutex_lock)(pthread_mutex_t *mutex) = NULL;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t *mutex) = NULL;
static int (*real_pthread_mutex_trylock)(pthread_mutex_t *mutex) = NULL;

/* Thread-local flag to prevent recursive hooking */
static __thread int lockdep_hooked = 0;

/**
 * Initialize the real pthread function pointers.
 */
static void lockdep_init_real_functions(void) {
    if (real_pthread_mutex_lock &&
        real_pthread_mutex_unlock &&
        real_pthread_mutex_trylock) {
        return;
    }

    real_pthread_mutex_lock =
        (int (*)(pthread_mutex_t *))dlsym(RTLD_NEXT, "pthread_mutex_lock");

    real_pthread_mutex_unlock =
        (int (*)(pthread_mutex_t *))dlsym(RTLD_NEXT, "pthread_mutex_unlock");

    real_pthread_mutex_trylock =
        (int (*)(pthread_mutex_t *))dlsym(RTLD_NEXT, "pthread_mutex_trylock");

    if (!real_pthread_mutex_lock ||
        !real_pthread_mutex_unlock ||
        !real_pthread_mutex_trylock) {
        lockdep_panic("[LOCKDEP] failed to resolve real pthread mutex symbols\n");
    }
}

/**
 * Constructor.
 * Initializes the real pthread functions and runtime flags.
 */
__attribute__((constructor))
static void lockdep_ctor(void) {
    const char *debug_env = getenv("LOCKDEP_DEBUG");
    g_debug_enabled = (debug_env && atoi(debug_env) != 0);
    lockdep_init_real_functions();
}

/**
 * Hooked pthread_mutex_lock.
 *
 * Flow:
 *   1. If we're already inside lockdep, call the real function directly.
 *   2. Try a non-blocking fast path with pthread_mutex_trylock.
 *   3. If the lock is busy, check for an actual deadlock before blocking.
 *   4. If no actual deadlock is detected, call the real blocking lock.
 *   5. On successful acquisition, update both actual-deadlock and
 *      potential-deadlock state.
 */
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!real_pthread_mutex_lock) {
        lockdep_init_real_functions();
    }

    if (lockdep_hooked) {
        return real_pthread_mutex_lock(mutex);
    }
    lockdep_hooked = 1;

    int rc = real_pthread_mutex_trylock(mutex);
    if (rc == 0) {
        lockdep_log_lock_event("lock", mutex, rc);
        lockdep_acquire_mutex(mutex, 0);
        lockdep_hooked = 0;
        return 0;
    }

    if (rc == EBUSY) {
        if (lockdep_before_blocking_mutex_lock(mutex)) {
            lockdep_hooked = 0;
            _exit(66);
        }

        rc = real_pthread_mutex_lock(mutex);
        if (rc == 0) {
            lockdep_log_lock_event("lock", mutex, rc);
            lockdep_acquire_mutex(mutex, 0);
        } else {
            lockdep_log_lock_event("lock-fail", mutex, rc);
            lockdep_cancel_wait();
        }

        lockdep_hooked = 0;
        return rc;
    }

    lockdep_log_lock_event("lock-fail", mutex, rc);
    lockdep_hooked = 0;
    return rc;
}

/**
 * Hooked pthread_mutex_unlock.
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!real_pthread_mutex_unlock) {
        lockdep_init_real_functions();
    }

    if (lockdep_hooked) {
        return real_pthread_mutex_unlock(mutex);
    }
    lockdep_hooked = 1;

    int rc = real_pthread_mutex_unlock(mutex);
    if (rc == 0) {
        lockdep_log_lock_event("unlock", mutex, rc);
        lockdep_release_mutex(mutex);
    } else {
        lockdep_log_lock_event("unlock-fail", mutex, rc);
    }

    lockdep_hooked = 0;
    return rc;
}

/**
 * Hooked pthread_mutex_trylock.
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!real_pthread_mutex_trylock) {
        lockdep_init_real_functions();
    }

    if (lockdep_hooked) {
        return real_pthread_mutex_trylock(mutex);
    }
    lockdep_hooked = 1;

    int rc = real_pthread_mutex_trylock(mutex);
    if (rc == 0) {
        lockdep_log_lock_event("trylock-success", mutex, rc);
        lockdep_acquire_mutex(mutex, 1);
    } else if (rc == EBUSY) {
        lockdep_log_lock_event("trylock-busy", mutex, rc);
    } else {
        lockdep_log_lock_event("trylock-fail", mutex, rc);
    }

    lockdep_hooked = 0;
    return rc;
}
