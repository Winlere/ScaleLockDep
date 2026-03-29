/*
 * TEST 1: Single Thread
 * Category: PURELY NON-DEADLOCK
 * Description: Single thread with lock. No concurrency, thus no deadlock possible.
 * Expected: Completes successfully
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&lock);
        printf("[T1] Critical section %d\n", i);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(void) {
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);
    pthread_join(t, NULL);
    printf("Test 1 PASSED: Single thread completed without deadlock\n");
    return 0;
}
