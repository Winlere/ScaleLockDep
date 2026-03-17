#ifndef LOCKDEP_H
#define LOCKDEP_H

#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Parameters */
#define LOCKDEP_MAX_LOCK_SLOTS 256
#define LOCKDEP_MAX_HELD_LOCK_SLOTS 64
#define LOCKDEP_MAX_THREAD_SLOTS 128

/* Definitions */
#define LOCKDEP_INVALID_SLOT (-1)

/*
 * Lock registry entry.
 * The array index is the lock slot.
 */
typedef struct {
    pthread_mutex_t *addr;
    int owner_thread_slot;   /* LOCKDEP_INVALID_SLOT if unlocked */
} lockdep_lock_slot_t;

/*
 * Thread registry entry.
 * The array index is the thread slot.
 */
typedef struct {
    pid_t tid;
    int waiting_on_lock_slot; /* LOCKDEP_INVALID_SLOT if not waiting */
} lockdep_thread_slot_t;

/*
 * Per-thread lock state.
 * Keeps track of the lock slots currently held by this thread.
 */
typedef struct {
    int held_lock_slots[LOCKDEP_MAX_HELD_LOCK_SLOTS];
    int held_lock_slot_count;
} lockdep_thread_state_t;

/* Shared state */
extern atomic_flag g_meta_lock;
extern lockdep_lock_slot_t g_lock_slots[LOCKDEP_MAX_LOCK_SLOTS];
extern lockdep_thread_slot_t g_thread_slots[LOCKDEP_MAX_THREAD_SLOTS];
extern int g_lock_slot_count;
extern int g_thread_slot_count;
extern __thread lockdep_thread_state_t tls_thread_state;
extern __thread int tls_thread_slot;
extern int g_debug_enabled;

/* Logging */
void lockdep_panic(const char *msg);
void lockdep_log_lock_event(const char *op, pthread_mutex_t *mutex, int rc);
void lockdep_log_held_lock_slots(int new_lock_slot);
void lockdep_report_potential_deadlock(int from_lock_slot,
                                       int to_lock_slot,
                                       const int *existing_chain,
                                       int existing_chain_len);
void lockdep_report_actual_deadlock(const int *thread_chain,
                                    const int *lock_chain,
                                    int edge_count);

/* State */
void lockdep_meta_lock(void);
void lockdep_meta_unlock(void);
int lockdep_lookup_lock_slot(pthread_mutex_t *mutex, int *lock_slot_out);
int lockdep_lookup_or_create_lock_slot(pthread_mutex_t *mutex);
void lockdep_push_held_lock_slot(int lock_slot);
void lockdep_remove_held_lock_slot(int lock_slot);
int lockdep_get_or_register_thread_slot(void);

/* Core */
void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock);
void lockdep_release_mutex(pthread_mutex_t *mutex);
int lockdep_before_blocking_mutex_lock(pthread_mutex_t *mutex);
void lockdep_cancel_wait(void);

/* Graph */
void lockdep_add_edge_and_check_cycle(int from_lock_slot, int to_lock_slot);

#endif /* LOCKDEP_H */
