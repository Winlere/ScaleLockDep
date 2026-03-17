#ifndef LOCKDEP_H
#define LOCKDEP_H

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>

// Maximum number of locks that can be tracked
#define LOCKDEP_MAX_LOCKS 256

// Maximum number of locks that can be held by a thread
#define LOCKDEP_MAX_HELD_LOCKS 32

// Entry for lock registry. Maps a mutex address to an ID.
typedef struct {
    pthread_mutex_t *addr;
    unsigned int id;
} lockdep_lock_entry_t;

// Entry for thread-local lock state. Maintains a stack of held lock IDs.
typedef struct {
    unsigned int held[LOCKDEP_MAX_HELD_LOCKS];
    unsigned int held_count;
} lockdep_thread_state_t;

// State
extern atomic_flag g_meta_lock;
extern lockdep_lock_entry_t g_locks[LOCKDEP_MAX_LOCKS];
extern int g_num_locks;
extern __thread lockdep_thread_state_t tls_state;

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

// Graph
void lockdep_add_edge_and_check_cycle(unsigned int from, unsigned int to);

#endif /* LOCKDEP_H */
