/*
 * bench_potential_edges.c — isolates lockdep potential-graph construction cost.
 *
 * Usage:
 *   ./bench_potential_edges.out <num_threads> <locks_per_thread> [iters]
 *
 * Each thread owns a *private* set of <locks_per_thread> mutexes; the sets are
 * disjoint across threads. Every iteration acquires the thread's locks in fixed
 * ascending order, then releases in descending order.
 *
 * Because lock sets are disjoint, application-level mutex contention is zero —
 * threads never block on each other's mutexes. Under LD_PRELOAD lockdep, the
 * measured overhead therefore tracks potential-deadlock graph construction
 * cost: held-stack maintenance plus per-acquire dependency edge insertion and
 * cycle check (or async enqueue, in rb mode).
 *
 * Total lock pairs = num_threads * locks_per_thread * iters.
 *
 * Note (lockdep limits):
 *   num_threads * locks_per_thread <= LOCKDEP_MAX_LOCK_SLOTS  (default 256)
 *   locks_per_thread              <= LOCKDEP_MAX_HELD_LOCK_SLOTS (default 64)
 *   num_threads                   <= LOCKDEP_MAX_THREAD_SLOTS (default 128)
 *
 * Output (one tab-separated line):
 *   threads  locks_per_thread  iters  wall_ns  total_lock_ops  lock_ops_per_sec
 */

#define _POSIX_C_SOURCE 199309L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static int  g_locks_per_thread;
static long g_iters;
static pthread_mutex_t **g_thread_locks;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void *worker(void *arg) {
    long tid = (long)arg;
    pthread_mutex_t *my = g_thread_locks[tid];
    for (long i = 0; i < g_iters; i++) {
        for (int l = 0; l < g_locks_per_thread; l++)
            pthread_mutex_lock(&my[l]);
        for (int l = g_locks_per_thread - 1; l >= 0; l--)
            pthread_mutex_unlock(&my[l]);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "usage: %s <num_threads> <locks_per_thread> [iters]\n",
                argv[0]);
        return 1;
    }

    int num_threads    = atoi(argv[1]);
    g_locks_per_thread = atoi(argv[2]);
    g_iters            = (argc == 4) ? atol(argv[3]) : 100000;

    if (num_threads <= 0 || g_locks_per_thread <= 0 || g_iters <= 0) {
        fprintf(stderr, "all arguments must be positive integers\n");
        return 1;
    }

    g_thread_locks = calloc((size_t)num_threads, sizeof(*g_thread_locks));
    if (!g_thread_locks) { perror("calloc"); return 1; }

    for (int t = 0; t < num_threads; t++) {
        g_thread_locks[t] = calloc((size_t)g_locks_per_thread,
                                   sizeof(pthread_mutex_t));
        if (!g_thread_locks[t]) { perror("calloc"); return 1; }
        for (int l = 0; l < g_locks_per_thread; l++)
            pthread_mutex_init(&g_thread_locks[t][l], NULL);
    }

    pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
    if (!threads) { perror("calloc"); return 1; }

    uint64_t t0 = now_ns();
    for (long i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, worker, (void *)i);
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);
    uint64_t t1 = now_ns();

    uint64_t wall_ns    = t1 - t0;
    uint64_t total_ops  = (uint64_t)num_threads
                        * (uint64_t)g_locks_per_thread
                        * (uint64_t)g_iters;
    double   ops_per_s  = (double)total_ops / ((double)wall_ns / 1e9);

    printf("%d\t%d\t%ld\t%lu\t%lu\t%.0f\n",
           num_threads, g_locks_per_thread, g_iters,
           (unsigned long)wall_ns, (unsigned long)total_ops, ops_per_s);

    for (int t = 0; t < num_threads; t++) {
        for (int l = 0; l < g_locks_per_thread; l++)
            pthread_mutex_destroy(&g_thread_locks[t][l]);
        free(g_thread_locks[t]);
    }
    free(g_thread_locks);
    free(threads);
    return 0;
}
