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
#include <stdint.h>
#include <stdatomic.h>

// Configurations.
#define LOCKDEP_MAX_LOCKS 256
#define LOCKDEP_MAX_HELD_LOCKS 32

// Map between lock addresses and their IDs.
typedef struct {
    pthread_mutex_t *addr;
    unsigned int id;
} lockdep_lock_entry_t;

// Per-thread state entry.
typedef struct {
    unsigned int held[LOCKDEP_MAX_HELD_LOCKS];
    unsigned long held_count;
} lockdep_thread_state_t;

// Global metadata lock.
static atomic_flag g_meta_lock = ATOMIC_FLAG_INIT;

// Global lock registry.
static lockdep_lock_entry_t g_locks[LOCKDEP_MAX_LOCKS];

// Global lock count.
static uint16_t g_num_locks = 0;

// Per-thread state.
static __thread lockdep_thread_state_t tls_state = {0};

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

// Per thread held locks log.
static void lockdep_log_held_context(unsigned int new_id) {
    char buf[512];
    int off = snprintf(buf, sizeof(buf),
                       "[LOCKDEP] tid=%d acquire id=%u held=[",
                       gettid(), (unsigned)new_id);

    if (off < 0) {
        return;
    }

    for (uint16_t i = 0; i < tls_state.held_count && off < (int)sizeof(buf); i++) {
        off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                        "%u%s",
                        (unsigned)tls_state.held[i],
                        ((unsigned long)(i + 1) < tls_state.held_count ? "," : ""));
    }

    if (off < (int)sizeof(buf)) {
        off += snprintf(buf + off, sizeof(buf) - (size_t)off, "]\n");
    }

    if (off > 0) {
        size_t len = ((size_t)off < sizeof(buf)) ? (size_t)off : sizeof(buf) - 1;
        size_t pos = 0;
        while (pos < len) {
            ssize_t w = write(2, buf + pos, len - pos);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            pos += (size_t)w;
        }
    }
}

// Global metadata lockup.
static void lockdep_meta_lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_meta_lock, memory_order_acquire)) {
        /* spin */
    }
}

// Global metadata unlock.
static void lockdep_meta_unlock(void) {
    atomic_flag_clear_explicit(&g_meta_lock, memory_order_release);
}

// Lookup a lock ID for the given mutex address.
static int lockdep_lookup_lock_id(pthread_mutex_t *mutex, unsigned int *out) {
    lockdep_meta_lock();

    for (uint16_t i = 0; i < g_num_locks; i++) {
        if (g_locks[i].addr == mutex) {
            *out = g_locks[i].id;
            lockdep_meta_unlock();
            return 1;
        }
    }

    lockdep_meta_unlock();
    return 0;
}

// Lookup or create a lock ID for the given mutex address.
static unsigned int lockdep_lookup_or_create_lock_id(pthread_mutex_t *mutex) {
    lockdep_meta_lock();

    for (uint16_t i = 0; i < g_num_locks; i++) {
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

    unsigned int new_id = g_num_locks;
    g_locks[g_num_locks].addr = mutex;
    g_locks[g_num_locks].id = new_id;
    g_num_locks++;

    lockdep_meta_unlock();
    return new_id;
}

// Push a held lock ID to the per-thread stack.
static void lockdep_push_held(unsigned int id) {
    if (tls_state.held_count >= LOCKDEP_MAX_HELD_LOCKS) {
        lockdep_panic("[LOCKDEP] held-lock stack overflow\n");
    }

    tls_state.held[tls_state.held_count++] = id;
}

// Remove a held lock ID from the per-thread stack.
static void lockdep_remove_held(unsigned int id) {
    for (int i = (int)tls_state.held_count - 1; i >= 0; i--) {
        if (tls_state.held[i] == id) {
            for (uint16_t j = (uint16_t)i; (unsigned long)(j + 1) < tls_state.held_count; j++) {
                tls_state.held[j] = tls_state.held[j + 1];
            }
            tls_state.held_count--;
            return;
        }
    }

    printf("[LOCKDEP] warning: unlock unknown-held lock id=%u tid=%d\n",
                 (unsigned)id, gettid());
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
    (void)via_trylock;

    unsigned int id = lockdep_lookup_or_create_lock_id(mutex);

    lockdep_log_held_context(id);

    lockdep_push_held(id);
}

// Mutex unlock hook.
static void lockdep_release_mutex(pthread_mutex_t *mutex) {
    unsigned int id;
    if (!lockdep_lookup_lock_id(mutex, &id)) {
        printf("[LOCKDEP] warning: unlock on unknown mutex=%p tid=%d\n",
                     (void *)mutex, gettid());
        return;
    }

    lockdep_remove_held(id);
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
