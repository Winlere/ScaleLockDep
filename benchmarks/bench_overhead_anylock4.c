/*
 * bench_overhead_anylock4.c — 4-lock any-acquire overhead microbenchmark
 *
 * Usage:
 *   ./bench_overhead_anylock4.out <num_threads> <iters_per_thread>
 *
 * Four shared locks. Each iteration a thread tries each lock in round-robin
 * order starting from lock[tid % 4], taking the first one available.
 * Falls back to blocking on the preferred lock if all four are busy.
 *
 * This allows up to 4 threads to run concurrently (one per lock).
 *
 * Output (tab-separated):
 *   threads  iters  wall_ns  total_ops  ops_per_sec
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define NUM_LOCKS 4

static pthread_mutex_t g_locks[NUM_LOCKS];
static long g_iters;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void *worker(void *arg) {
    long tid = (long)arg;
    int preferred = (int)(tid % NUM_LOCKS);

    for (long i = 0; i < g_iters; i++) {
        int acquired = -1;
        for (int k = 0; k < NUM_LOCKS; k++) {
            int idx = (preferred + k) % NUM_LOCKS;
            if (pthread_mutex_trylock(&g_locks[idx]) == 0) {
                acquired = idx;
                break;
            }
        }
        if (acquired >= 0) {
            pthread_mutex_unlock(&g_locks[acquired]);
        } else {
            /* All busy: block on preferred lock */
            pthread_mutex_lock(&g_locks[preferred]);
            pthread_mutex_unlock(&g_locks[preferred]);
        }
    }
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

    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_init(&g_locks[i], NULL);

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

    printf("%d\t%ld\t%lu\t%lu\t%.0f\n",
           num_threads, g_iters,
           (unsigned long)wall_ns, (unsigned long)total_ops, ops_per_sec);

    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_destroy(&g_locks[i]);
    free(threads);
    return 0;
}
