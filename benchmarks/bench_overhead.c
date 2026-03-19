/*
 * bench_overhead.c — overhead microbenchmark for ScaleLockDep
 *
 * Usage:
 *   ./bench_overhead.out <num_threads> <num_locks> <iters_per_thread>
 *
 * Each thread acquires/releases all locks in order for <iters> iterations.
 * With num_locks == 1 and many threads: high contention.
 * With num_locks == num_threads (each thread owns its own lock): low contention,
 * isolates lockdep tracking overhead from synchronization overhead.
 *
 * Output (one line, tab-separated):
 *   threads  locks  iters  wall_ns  total_lock_ops  lock_ops_per_sec
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static pthread_mutex_t *g_locks;
static int g_num_locks;
static long g_iters;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void *worker(void *arg) {
    long tid = (long)arg;
    for (long i = 0; i < g_iters; i++) {
        /* Each thread cycles through locks starting at its own offset
         * to prevent artificially uniform lock ordering artifacts. */
        for (int l = 0; l < g_num_locks; l++) {
            int idx = (tid + l) % g_num_locks;
            pthread_mutex_lock(&g_locks[idx]);
            pthread_mutex_unlock(&g_locks[idx]);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <num_threads> <num_locks> <iters>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    g_num_locks     = atoi(argv[2]);
    g_iters         = atol(argv[3]);

    if (num_threads <= 0 || g_num_locks <= 0 || g_iters <= 0) {
        fprintf(stderr, "all arguments must be positive integers\n");
        return 1;
    }

    g_locks = calloc(g_num_locks, sizeof(pthread_mutex_t));
    if (!g_locks) { perror("calloc"); return 1; }
    for (int i = 0; i < g_num_locks; i++)
        pthread_mutex_init(&g_locks[i], NULL);

    pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
    if (!threads) { perror("calloc"); return 1; }

    uint64_t t0 = now_ns();
    for (long i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, worker, (void *)i);
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);
    uint64_t t1 = now_ns();

    uint64_t wall_ns      = t1 - t0;
    uint64_t total_ops    = (uint64_t)num_threads * (uint64_t)g_iters * (uint64_t)g_num_locks;
    double   ops_per_sec  = (double)total_ops / ((double)wall_ns / 1e9);

    /* Print header on first run (when threads==1 hint, but always print data) */
    printf("%d\t%d\t%ld\t%lu\t%lu\t%.0f\n",
           num_threads, g_num_locks, g_iters,
           (unsigned long)wall_ns, (unsigned long)total_ops, ops_per_sec);

    for (int i = 0; i < g_num_locks; i++)
        pthread_mutex_destroy(&g_locks[i]);
    free(g_locks);
    free(threads);
    return 0;
}
