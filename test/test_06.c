/*
 * Description: Threads mostly do work without locks. Lock only used for occasional
 * synchronization. Minimal contention and lock time.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t sync_lock = PTHREAD_MUTEX_INITIALIZER;
static int counter = 0;

void* worker(void* arg) {
    long tid = (long)arg;
    for (int iter = 0; iter < 50; iter++) {
        // Long computation without locks
        volatile int x = 0;
        for (int i = 0; i < 10000; i++) {
            x += i;
        }

        // Brief critical section
        pthread_mutex_lock(&sync_lock);
        counter++;
        if (counter % 10 == 0) {
        }
        pthread_mutex_unlock(&sync_lock);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, worker, (void*)1);
    pthread_create(&t2, NULL, worker, (void*)2);
    pthread_create(&t3, NULL, worker, (void*)3);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    printf("Test 6 PASSED: Lock-free work completed without deadlock (counter=%d)\n", counter);
    return 0;
}
