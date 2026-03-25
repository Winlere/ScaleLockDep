/*
 * bench_latency.c — per-operation latency microbenchmark for ScaleLockDep
 *
 * Usage:
 *   ./bench_latency.out <num_threads> <iters_per_thread>
 *
 * Measures the latency (nanoseconds) of each pthread_mutex_lock and
 * pthread_mutex_unlock call individually using clock_gettime(CLOCK_MONOTONIC).
 * Reports average lock latency, unlock latency, and total pair latency.
 *
 * Note: clock_gettime adds ~15-25 ns measurement overhead per call,
 * consistent across baseline and lockdep, so the *difference* between
 * the two conditions is accurate.
 *
 * Output (tab-separated):
 *   threads  iters  avg_lock_ns  avg_unlock_ns  avg_pair_ns
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static long g_iters;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

typedef struct {
    uint64_t lock_total_ns;
    uint64_t unlock_total_ns;
} thread_result_t;

static thread_result_t *g_results;

static void *worker(void *arg) {
    long tid = (long)arg;
    uint64_t lock_sum = 0, unlock_sum = 0;

    for (long i = 0; i < g_iters; i++) {
        uint64_t t0 = now_ns();
        pthread_mutex_lock(&g_lock);
        uint64_t t1 = now_ns();
        pthread_mutex_unlock(&g_lock);
        uint64_t t2 = now_ns();

        lock_sum   += (t1 - t0);
        unlock_sum += (t2 - t1);
    }

    g_results[tid].lock_total_ns   = lock_sum;
    g_results[tid].unlock_total_ns = unlock_sum;
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <num_threads> <iters>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    g_iters         = atol(argv[2]);

    if (num_threads <= 0 || g_iters <= 0) {
        fprintf(stderr, "all arguments must be positive integers\n");
        return 1;
    }

    g_results = calloc(num_threads, sizeof(thread_result_t));
    if (!g_results) { perror("calloc"); return 1; }

    pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
    if (!threads) { perror("calloc"); return 1; }

    for (long i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, worker, (void *)i);
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    /* Aggregate across all threads */
    uint64_t total_lock_ns   = 0;
    uint64_t total_unlock_ns = 0;
    uint64_t total_ops       = (uint64_t)num_threads * (uint64_t)g_iters;

    for (int i = 0; i < num_threads; i++) {
        total_lock_ns   += g_results[i].lock_total_ns;
        total_unlock_ns += g_results[i].unlock_total_ns;
    }

    double avg_lock_ns   = (double)total_lock_ns   / (double)total_ops;
    double avg_unlock_ns = (double)total_unlock_ns / (double)total_ops;
    double avg_pair_ns   = avg_lock_ns + avg_unlock_ns;

    printf("%d\t%ld\t%.1f\t%.1f\t%.1f\n",
           num_threads, g_iters,
           avg_lock_ns, avg_unlock_ns, avg_pair_ns);

    free(g_results);
    free(threads);
    return 0;
}
