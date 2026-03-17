#include "lockdep.h"

#include <dlfcn.h>
#include <errno.h>

// Function pointers to real pthread functions
static int (*real_pthread_mutex_lock)(pthread_mutex_t *mutex) = NULL;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t *mutex) = NULL;
static int (*real_pthread_mutex_trylock)(pthread_mutex_t *mutex) = NULL;

// Thread-local flag to prevent recursive hooking
static __thread int lockdep_hooked = 0;

/**
 * Initialize the function pointers to the real pthread functions.
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
 * Initialize the real function pointers when the library is loaded.
 */
__attribute__((constructor))
static void lockdep_ctor(void) {
    lockdep_init_real_functions();
}

/**
 * Hooked functions of system pthread functions.
 * They: 1. Check if the real function pointers are ready;
 *       2. Prevent recursive hooking by checking the thread-local flag;
 *       3. Try to acquire the lock with trylock.
 *       4. If successful, call the core functions to update the dependency graph.
 *       5. If the lock is busy, call the core function to check for actual deadlock before blocking.
 *       6. If there is no deadlock, try to acquire the lock again and update the dependency graph if successful.
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
        lockdep_log("lock", mutex, rc);
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
            lockdep_log("lock", mutex, rc);
            lockdep_acquire_mutex(mutex, 0);
        } else {
            lockdep_log("lock-fail", mutex, rc);
            lockdep_cancel_wait();
        }

        lockdep_hooked = 0;
        return rc;
    }

    lockdep_log("lock-fail", mutex, rc);
    lockdep_hooked = 0;
    return rc;
}

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
        lockdep_log("unlock", mutex, rc);
        lockdep_release_mutex(mutex);
    } else {
        lockdep_log("unlock-fail", mutex, rc);
    }

    lockdep_hooked = 0;
    return rc;
}

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
        lockdep_log("trylock-success", mutex, rc);
        lockdep_acquire_mutex(mutex, 1);
    } else if (rc == EBUSY) {
        lockdep_log("trylock-busy", mutex, rc);
    } else {
        lockdep_log("trylock-fail", mutex, rc);
    }

    lockdep_hooked = 0;
    return rc;
}
