/*
 * TEST 13: Dubious Dynamic Lock Selection
 * Category: DUBIOUS NON-DEADLOCK
 * Description: Lock acquisition order is determined dynamically based on thread ID.
 * Works correctly if the ID-based ordering happens to prevent cycles,
 * but is fragile and not obviously safe.
 * Expected: May complete (if ID ordering prevents deadlock)
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    long tid = (long)arg;

    for (int i = 0; i < 30; i++) {
        // Dynamically select lock order based on thread ID
        // This creates an implicit total order that might prevent deadlock
        // But it's not obvious or maintainable
        if (tid % 3 == 0) {
            pthread_mutex_lock(&A);
            usleep(5000);
            pthread_mutex_lock(&B);
            pthread_mutex_unlock(&B);
            pthread_mutex_unlock(&A);
        } else if (tid % 3 == 1) {
            pthread_mutex_lock(&B);
            usleep(5000);
            pthread_mutex_lock(&C);
            pthread_mutex_unlock(&C);
            pthread_mutex_unlock(&B);
        } else {
            pthread_mutex_lock(&C);
            usleep(5000);
            pthread_mutex_lock(&A);
            pthread_mutex_unlock(&A);
            pthread_mutex_unlock(&C);
        }
        printf("[T%ld] Completed iteration %d\n", tid, i);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, worker, (void*)0);
    pthread_create(&t2, NULL, worker, (void*)1);
    pthread_create(&t3, NULL, worker, (void*)2);

    sleep(2);
    printf("Test 13: DUBIOUS DYNAMIC (may or may not deadlock based on thread interleaving)\n");
    return 0;
}
