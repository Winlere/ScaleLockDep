/*
 * TEST 2: Two Threads, One Lock
 * Category: PURELY NON-DEADLOCK
 * Description: Multiple threads contending for a single lock. Sequential access
 * prevents deadlock (only one lock, so no circular wait possible).
 * Expected: Completes successfully
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    long tid = (long)arg;
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&lock);
        printf("[T%ld] Critical section %d\n", tid, i);
        usleep(1000);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, (void*)1);
    pthread_create(&t2, NULL, worker, (void*)2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Test 2 PASSED: Two threads, one lock completed without deadlock\n");
    return 0;
}
