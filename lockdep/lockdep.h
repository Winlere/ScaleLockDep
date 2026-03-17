#ifndef LOCKDEP_H
#define LOCKDEP_H

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>

// Parameters
#define LOCKDEP_MAX_LOCKS 256
#define LOCKDEP_MAX_HELD_LOCKS 32
#define LOCKDEP_MAX_THREADS 128

// Entry for a lock registry.
typedef struct {
    pthread_mutex_t *addr;
    unsigned int id;
    int owner_slot;  // -1 if not owned
} lockdep_lock_entry_t;

// Entry for a thread registry.
typedef struct {
    pid_t tid;
    unsigned int waiting_on;  // -1 if not waiting
} lockdep_thread_entry_t;

// Entry for thread-local lock state. Maintains a stack of held lock IDs.
typedef struct {
    unsigned int held[LOCKDEP_MAX_HELD_LOCKS];
    unsigned int held_count;
} lockdep_thread_state_t;

// State
extern atomic_flag g_meta_lock;
extern lockdep_lock_entry_t g_locks[LOCKDEP_MAX_LOCKS];
extern lockdep_thread_entry_t g_threads[LOCKDEP_MAX_THREADS];
extern int g_num_locks;
extern int g_num_threads;
extern __thread lockdep_thread_state_t tls_state;
extern __thread int tls_thread_slot;

// Log
void lockdep_panic(const char *msg);
void lockdep_log(const char *op, pthread_mutex_t *mutex, int rc);
void lockdep_log_held_context(unsigned int new_id);

// State
void lockdep_meta_lock(void);
void lockdep_meta_unlock(void);
int lockdep_lookup_lock_id(pthread_mutex_t *mutex, unsigned int *out);
unsigned int lockdep_lookup_or_create_lock_id(pthread_mutex_t *mutex);
void lockdep_push_held(unsigned int id);
void lockdep_remove_held(unsigned int id);

// Core
void lockdep_acquire_mutex(pthread_mutex_t *mutex, int via_trylock);
void lockdep_release_mutex(pthread_mutex_t *mutex);
int lockdep_get_or_register_thread_slot(void);
int lockdep_before_blocking_mutex_lock(pthread_mutex_t *mutex);
void lockdep_cancel_wait(void);
void lockdep_report_actual_deadlock(unsigned int self_slot, unsigned int target_lock, int owner_slot);

// Graph
void lockdep_add_edge_and_check_cycle(unsigned int from, unsigned int to);

#endif /* LOCKDEP_H */
