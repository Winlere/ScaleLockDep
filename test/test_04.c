/*
 * TEST 4: Ordered Locking - Multiple Threads, 3 Locks
 * Category: PURELY NON-DEADLOCK
 * Description: Like the benchmark correct_40threads_3locks, all threads
 * lock in order A->B->C, preventing deadlock.
 * Expected: Completes successfully
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

static const int NUM_THREADS = 8;
static const int ITERS = 100;

void* worker(void* arg) {
    long tid = (long)arg;
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&A);
        pthread_mutex_lock(&B);
        pthread_mutex_lock(&C);

        if (i % 30 == 0) {
            printf("[T%ld] progress %d\n", tid, i);
        }

        pthread_mutex_unlock(&C);
        pthread_mutex_unlock(&B);
        pthread_mutex_unlock(&A);
    }
    return NULL;
}

int main(void) {
    pthread_t th[NUM_THREADS];
    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&th[i], NULL, worker, (void*)i);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(th[i], NULL);
    }
    printf("Test 4 PASSED: Ordered locking 3 locks completed without deadlock\n");
    return 0;
}
