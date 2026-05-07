#ifndef LOCKDEP_H
#define LOCKDEP_H

#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Parameters */
#ifndef LOCKDEP_MAX_LOCK_SLOTS
#define LOCKDEP_MAX_LOCK_SLOTS 256
#endif
#ifndef LOCKDEP_MAX_HELD_LOCK_SLOTS
#define LOCKDEP_MAX_HELD_LOCK_SLOTS 64
#endif
#ifndef LOCKDEP_MAX_THREAD_SLOTS
#define LOCKDEP_MAX_THREAD_SLOTS 128
#endif
#ifndef LOCKDEP_RB_CAPACITY
#define LOCKDEP_RB_CAPACITY 4096
#endif
#ifndef LOCKDEP_TLS_LOCK_CACHE_SIZE
#define LOCKDEP_TLS_LOCK_CACHE_SIZE 64
#endif
#define LOCKDEP_GRAPH_WORDS ((LOCKDEP_MAX_LOCK_SLOTS + 63) / 64)

/* Definitions */
#define LOCKDEP_INVALID_SLOT (-1)
#define LOCKDEP_LOCK_SLOT_WORD_INDEX(lock_slot) ((lock_slot) / 64)
#define LOCKDEP_LOCK_SLOT_BIT_MASK(lock_slot) (1ULL << ((lock_slot) % 64))

typedef enum {
    LOCKDEP_MODE_GLOBAL = 0,
    LOCKDEP_MODE_RB = 1,
} lockdep_mode_t;

typedef struct {
    int thread_slot;
    uintptr_t acquire_pc;
} lockdep_edge_info_t;

#if defined(__GNUC__) || defined(__clang__)
#define LOCKDEP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LOCKDEP_CALLSITE() \
    ((uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)))
#else
#define LOCKDEP_UNLIKELY(x) (x)
#define LOCKDEP_CALLSITE() ((uintptr_t)0)
#endif

/*
 * Lock registry entry.
 * The array index is the lock slot.
 */
typedef struct {
    pthread_mutex_t *addr;
    _Atomic int owner_thread_slot;   /* LOCKDEP_INVALID_SLOT if unlocked */
    _Atomic uintptr_t owner_acquire_pc;
} lockdep_lock_slot_t;

/*
 * Thread registry entry.
 * The array index is the thread slot.
 */
typedef struct {
    pid_t tid;
    _Atomic int waiting_on_lock_slot; /* LOCKDEP_INVALID_SLOT if not waiting */
    _Atomic uintptr_t waiting_on_pc;
} lockdep_thread_slot_t;

/*
 * Per-thread lock state.
 * Keeps track of the lock slots currently held by this thread.
 */
typedef struct {
    int held_lock_slots[LOCKDEP_MAX_HELD_LOCK_SLOTS];
    int held_lock_slot_count;
    unsigned short held_lock_refcounts[LOCKDEP_MAX_LOCK_SLOTS];
    uint64_t held_lock_bits[LOCKDEP_GRAPH_WORDS];
} lockdep_thread_state_t;

typedef struct {
    pthread_mutex_t *mutex;
    int lock_slot;
} lockdep_lock_slot_cache_entry_t;

/* Shared state */
extern atomic_flag g_meta_lock;
extern lockdep_lock_slot_t g_lock_slots[LOCKDEP_MAX_LOCK_SLOTS];
extern lockdep_thread_slot_t g_thread_slots[LOCKDEP_MAX_THREAD_SLOTS];
extern _Atomic int g_lock_slot_count;
extern _Atomic int g_thread_slot_count;
extern __thread lockdep_thread_state_t tls_thread_state;
extern __thread int tls_thread_slot;
extern __thread lockdep_lock_slot_cache_entry_t
    tls_lock_slot_cache[LOCKDEP_TLS_LOCK_CACHE_SIZE];
extern int g_debug_enabled;
extern int g_report_sites_enabled;
extern lockdep_mode_t g_lockdep_mode;

/* Logging */
void lockdep_panic(const char *msg);
void lockdep_log_lock_event(const char *op, pthread_mutex_t *mutex, int rc);
void lockdep_log_held_lock_slots(int new_lock_slot);
void lockdep_report_potential_deadlock(int from_lock_slot,
                                       int to_lock_slot,
                                       const int *existing_chain,
                                       int existing_chain_len,
                                       const lockdep_edge_info_t *new_edge_info,
                                       const lockdep_edge_info_t *existing_edge_infos);
void lockdep_report_actual_deadlock(const int *thread_chain,
                                    const int *lock_chain,
                                    int edge_count);

/* State */
void lockdep_meta_lock(void);
void lockdep_meta_unlock(void);
int lockdep_lookup_lock_slot(pthread_mutex_t *mutex, int *lock_slot_out);
int lockdep_lookup_or_create_lock_slot(pthread_mutex_t *mutex);
int lockdep_lookup_lock_slot_cached(pthread_mutex_t *mutex, int *lock_slot_out);
int lockdep_lookup_or_create_lock_slot_cached(pthread_mutex_t *mutex);
void lockdep_push_held_lock_slot(int lock_slot);
void lockdep_remove_held_lock_slot(int lock_slot);
int lockdep_get_or_register_thread_slot(void);

/* Core */
void lockdep_acquire_mutex(pthread_mutex_t *mutex,
                           int clear_wait_state,
                           uintptr_t acquire_pc);
void lockdep_release_mutex(pthread_mutex_t *mutex);
int lockdep_before_blocking_mutex_lock(pthread_mutex_t *mutex, uintptr_t waiting_pc);
void lockdep_cancel_wait(void);

/* Graph */
void lockdep_add_edge_and_check_cycle(int from_lock_slot,
                                      int to_lock_slot,
                                      int thread_slot,
                                      uintptr_t acquire_pc);
int lockdep_global_has_unknown_predecessor(const lockdep_thread_state_t *thread_state,
                                           int new_lock_slot);

/* Potential backend */
const char *lockdep_mode_name(lockdep_mode_t mode);
void lockdep_set_mode_from_env(const char *mode_env);
void lockdep_potential_init(void);
void lockdep_potential_shutdown(void);
void lockdep_potential_on_acquire(int thread_slot,
                                  int new_lock_slot,
                                  const lockdep_thread_state_t *thread_state,
                                  uintptr_t acquire_pc);

static inline void lockdep_debug_log_lock_event(const char *op,
                                                pthread_mutex_t *mutex,
                                                int rc) {
    if (LOCKDEP_UNLIKELY(g_debug_enabled)) {
        lockdep_log_lock_event(op, mutex, rc);
    }
}

static inline void lockdep_debug_log_held_lock_slots(int new_lock_slot) {
    if (LOCKDEP_UNLIKELY(g_debug_enabled)) {
        lockdep_log_held_lock_slots(new_lock_slot);
    }
}

static inline int lockdep_current_thread_slot(void) {
    if (tls_thread_slot != LOCKDEP_INVALID_SLOT) {
        return tls_thread_slot;
    }

    return lockdep_get_or_register_thread_slot();
}

static inline uintptr_t lockdep_capture_callsite_if_enabled(void) {
    if (!LOCKDEP_UNLIKELY(g_report_sites_enabled)) {
        return (uintptr_t)0;
    }

    return LOCKDEP_CALLSITE();
}

static inline void lockdep_store_owner_acquire_pc(int lock_slot, uintptr_t acquire_pc) {
    if (!LOCKDEP_UNLIKELY(g_report_sites_enabled)) {
        return;
    }

    atomic_store_explicit(&g_lock_slots[lock_slot].owner_acquire_pc,
                          acquire_pc,
                          memory_order_release);
}

static inline void lockdep_clear_owner_acquire_pc(int lock_slot) {
    if (!LOCKDEP_UNLIKELY(g_report_sites_enabled)) {
        return;
    }

    atomic_store_explicit(&g_lock_slots[lock_slot].owner_acquire_pc,
                          (uintptr_t)0,
                          memory_order_release);
}

static inline void lockdep_note_thread_holds_lock_slot(int lock_slot) {
    if (g_lockdep_mode != LOCKDEP_MODE_RB) {
        return;
    }

    if (tls_thread_state.held_lock_refcounts[lock_slot]++ == 0) {
        tls_thread_state.held_lock_bits[LOCKDEP_LOCK_SLOT_WORD_INDEX(lock_slot)] |=
            LOCKDEP_LOCK_SLOT_BIT_MASK(lock_slot);
    }
}

static inline void lockdep_note_thread_releases_lock_slot(int lock_slot) {
    if (g_lockdep_mode != LOCKDEP_MODE_RB) {
        return;
    }

    if (tls_thread_state.held_lock_refcounts[lock_slot] == 0) {
        return;
    }

    if (--tls_thread_state.held_lock_refcounts[lock_slot] == 0) {
        tls_thread_state.held_lock_bits[LOCKDEP_LOCK_SLOT_WORD_INDEX(lock_slot)] &=
            ~LOCKDEP_LOCK_SLOT_BIT_MASK(lock_slot);
    }
}

static inline int lockdep_lookup_lock_slot_cached_fast(pthread_mutex_t *mutex,
                                                       int *lock_slot_out) {
    unsigned int idx = ((uintptr_t)mutex >> 3) & (LOCKDEP_TLS_LOCK_CACHE_SIZE - 1);

    if (tls_lock_slot_cache[idx].mutex == mutex) {
        *lock_slot_out = tls_lock_slot_cache[idx].lock_slot;
        return 1;
    }

    return lockdep_lookup_lock_slot_cached(mutex, lock_slot_out);
}

static inline int lockdep_lookup_or_create_lock_slot_cached_fast(pthread_mutex_t *mutex) {
    unsigned int idx = ((uintptr_t)mutex >> 3) & (LOCKDEP_TLS_LOCK_CACHE_SIZE - 1);

    if (tls_lock_slot_cache[idx].mutex == mutex) {
        return tls_lock_slot_cache[idx].lock_slot;
    }

    return lockdep_lookup_or_create_lock_slot_cached(mutex);
}

static inline void lockdep_acquire_top_level_mutex_fast(pthread_mutex_t *mutex,
                                                        uintptr_t acquire_pc) {
    int self_thread_slot = lockdep_current_thread_slot();
    int lock_slot = lockdep_lookup_or_create_lock_slot_cached_fast(mutex);

    tls_thread_state.held_lock_slots[0] = lock_slot;
    tls_thread_state.held_lock_slot_count = 1;
    lockdep_note_thread_holds_lock_slot(lock_slot);
    lockdep_store_owner_acquire_pc(lock_slot, acquire_pc);
    atomic_store_explicit(&g_lock_slots[lock_slot].owner_thread_slot,
                          self_thread_slot,
                          memory_order_release);
}

static inline int lockdep_release_top_level_mutex_fast(pthread_mutex_t *mutex) {
    int lock_slot;
    int expected_owner_thread_slot;

    if (tls_thread_state.held_lock_slot_count != 1) {
        return 0;
    }

    lock_slot = tls_thread_state.held_lock_slots[0];
    if (g_lock_slots[lock_slot].addr != mutex) {
        return 0;
    }

    expected_owner_thread_slot = lockdep_current_thread_slot();
    if (atomic_compare_exchange_strong_explicit(&g_lock_slots[lock_slot].owner_thread_slot,
                                                &expected_owner_thread_slot,
                                                LOCKDEP_INVALID_SLOT,
                                                memory_order_acq_rel,
                                                memory_order_acquire)) {
        lockdep_clear_owner_acquire_pc(lock_slot);
    }
    lockdep_note_thread_releases_lock_slot(lock_slot);
    tls_thread_state.held_lock_slot_count = 0;
    return 1;
}

/* Hook control */
void lockdep_set_current_thread_internal(int is_internal);
int lockdep_current_thread_is_internal(void);

#endif /* LOCKDEP_H */
