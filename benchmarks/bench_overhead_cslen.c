/*
 * bench_overhead_cslen.c — critical section length overhead microbenchmark
 *
 * Usage:
 *   ./bench_overhead_cslen.out <num_threads> <iters_per_thread> <cs_ns>
 *
 * 1 shared lock, <num_threads> threads. Each iteration: lock, busy-spin for
 * <cs_ns> nanoseconds inside the critical section, then unlock.
 *
 * The busy-spin is calibrated at startup so that the inner loop closely
 * approximates the requested hold time.
 *
 * Output (tab-separated):
 *   threads  iters  cs_ns  wall_ns  total_ops  ops_per_sec
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static long g_iters;
static long g_spin_iters; /* calibrated loop count for requested cs_ns */

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Prevent the compiler from optimizing away the busy loop. */
static volatile int g_sink;

static void busy_spin(long n) {
    int s = 0;
    for (long i = 0; i < n; i++)
        s += (int)i;
    g_sink = s;
}

/* Calibrate: find loop count that takes approximately target_ns. */
static long calibrate(long target_ns) {
    if (target_ns <= 0)
        return 0;

    /* Warm up */
    busy_spin(1000);

    /* Measure a trial run */
    long trial = 10000;
    uint64_t t0 = now_ns();
    busy_spin(trial);
    uint64_t elapsed = now_ns() - t0;

    if (elapsed == 0)
        elapsed = 1;

    long result = (long)((double)trial * (double)target_ns / (double)elapsed);
    if (result < 1)
        result = 1;
    return result;
}

static void *worker(void *arg) {
    (void)arg;
    for (long i = 0; i < g_iters; i++) {
        pthread_mutex_lock(&g_lock);
        busy_spin(g_spin_iters);
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <num_threads> <iters> <cs_ns>\n", argv[0]);
        return 1;
    }

    int  num_threads = atoi(argv[1]);
    g_iters          = atol(argv[2]);
    long cs_ns       = atol(argv[3]);

    if (num_threads <= 0 || g_iters <= 0 || cs_ns < 0) {
        fprintf(stderr, "num_threads and iters must be positive; cs_ns >= 0\n");
        return 1;
    }

    g_spin_iters = calibrate(cs_ns);

    pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
    if (!threads) { perror("calloc"); return 1; }

    uint64_t t0 = now_ns();
    for (long i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, worker, (void *)i);
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);
    uint64_t t1 = now_ns();

    uint64_t wall_ns     = t1 - t0;
    uint64_t total_ops   = (uint64_t)num_threads * (uint64_t)g_iters;
    double   ops_per_sec = (double)total_ops / ((double)wall_ns / 1e9);

    printf("%d\t%ld\t%ld\t%lu\t%lu\t%.0f\n",
           num_threads, g_iters, cs_ns,
           (unsigned long)wall_ns, (unsigned long)total_ops, ops_per_sec);

    free(threads);
    return 0;
}
