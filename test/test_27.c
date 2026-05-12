/*
 * Description: 4 threads each use a fixed but TID-seeded permutation of 4
 * locks (computed via an LCG shuffle). Different permutations create
 * conflicting dependency edges in the lock graph; the detector exits early
 * (rc=66) when it finds the first potential cycle. An actual hang depends on
 * scheduling, but the ordering hazard is structurally guaranteed.
 * Expected: potential deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 4
#define NUM_LOCKS   4
#define ITERATIONS  30

static pthread_mutex_t locks[NUM_LOCKS];

static void shuffle(int *arr, int n, unsigned int seed) {
    for (int i = 0; i < n; i++) arr[i] = i;
    unsigned int r = seed;
    for (int i = n - 1; i > 0; i--) {
        r = r * 1664525u + 1013904223u; /* LCG */
        int j = (int)(r % (unsigned)(i + 1));
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

void* worker(void* arg) {
    int tid = (int)(long)arg;
    int order[NUM_LOCKS];
    shuffle(order, NUM_LOCKS, (unsigned)(tid + 1) * 31337u);
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < NUM_LOCKS; i++)     pthread_mutex_lock(&locks[order[i]]);
        for (int i = NUM_LOCKS - 1; i >= 0; i--) pthread_mutex_unlock(&locks[order[i]]);
    }
    return NULL;
}

int main(void) {
    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_init(&locks[i], NULL);
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void*)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("Test 27 completed: seeded-random lock ordering done\n");
    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_destroy(&locks[i]);
    return 0;
}
