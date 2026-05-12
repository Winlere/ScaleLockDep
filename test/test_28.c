/*
 * Description: 5 "safe" threads build the dependency chain L0→L1→L2→L3→L4
 * through consistent ordered acquisition. One "buggy" thread acquires L4
 * then L0, adding the back-edge L4→L0 and completing the cycle. The
 * detector flags this potential deadlock (rc=66); an actual hang requires
 * the buggy thread to race with a safe thread holding L0.
 * Expected: potential deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define NUM_LOCKS    5
#define SAFE_THREADS 5
#define ITERATIONS   2000

static pthread_mutex_t L[NUM_LOCKS];

void* safe_worker(void* arg) {
    (void)arg;
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < NUM_LOCKS; i++)     pthread_mutex_lock(&L[i]);
        for (int i = NUM_LOCKS - 1; i >= 0; i--) pthread_mutex_unlock(&L[i]);
    }
    return NULL;
}

void* buggy_worker(void* arg) {
    (void)arg;
    /* Acquires the last lock first, then the first — closes the cycle */
    for (int iter = 0; iter < ITERATIONS; iter++) {
        pthread_mutex_lock(&L[NUM_LOCKS - 1]);
        pthread_mutex_lock(&L[0]);
        pthread_mutex_unlock(&L[0]);
        pthread_mutex_unlock(&L[NUM_LOCKS - 1]);
    }
    return NULL;
}

int main(void) {
    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_init(&L[i], NULL);
    pthread_t threads[SAFE_THREADS + 1];
    for (int i = 0; i < SAFE_THREADS; i++)
        pthread_create(&threads[i], NULL, safe_worker, (void*)(long)i);
    pthread_create(&threads[SAFE_THREADS], NULL, buggy_worker, NULL);
    for (int i = 0; i <= SAFE_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("Test 28 completed: long chain with single inversion done\n");
    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_destroy(&L[i]);
    return 0;
}
