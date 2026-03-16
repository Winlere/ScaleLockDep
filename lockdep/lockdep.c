#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdarg.h>

// Real function pointers to systel pthread library.
static int (*real_pthread_mutex_lock)(pthread_mutex_t *mutex) = NULL;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t *mutex) = NULL;
static int (*real_pthread_mutex_trylock)(pthread_mutex_t *mutex) = NULL;

// Per-thread flag to avoid recursive hook.
static __thread int lockdep_hooked = 0;

// Panic exit.
static void lockdep_panic(const char *msg) {
    perror(msg);
    _exit(1);
}

// Operation log.
static void lockdep_log(const char *op, pthread_mutex_t *mutex, int rc) {
    printf("[LOCKDEP] tid=%d op=%s mutex=%p rc=%d\n", gettid(), op, (void *)mutex, rc);
}

// Initialize real function pointers using dlsym.
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

// Constructor.
__attribute__((constructor))
static void lockdep_ctor(void) {
    lockdep_init_real_functions();
    printf("[LOCKDEP] lockdep.so loaded, pid=%d\n", getpid());
    return;
}

// Mutex lock hook.
static void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock) {
    (void)mutex;
    (void)via_trylock;
}

// Mutex unlock hook.
static void lockdep_release_mutex(pthread_mutex_t *mutex) {
    (void)mutex;
}

// Hooked function.
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (lockdep_hooked) {
        return real_pthread_mutex_lock(mutex);
    }
    lockdep_hooked = 1;

    int rc = real_pthread_mutex_lock(mutex);
    if (rc == 0) {
        lockdep_log("lock", mutex, rc);
        lockdep_acquire_mutex(mutex, 0);
    } else {
        lockdep_log("lock-fail", mutex, rc);
    }

    lockdep_hooked = 0;
    return rc;
}

// Hooked function.
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
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

// Hooked function.
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
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
