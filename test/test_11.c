/*
 * TEST 11: Dubious Random Lock Order
 * Category: DUBIOUS NON-DEADLOCK
 * Description: Thread randomly selects lock order. This is unsafe in general,
 * but with only two locks and the specific seed, it might not deadlock in practice.
 * In production, this would be risky and could deadlock with different timing.
 * Expected: May complete (lucky) or deadlock (unlucky)
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    long tid = (long)arg;
    srand(tid);  // Different seed per thread

    for (int i = 0; i < 30; i++) {
        int order = rand() % 2;
        if (order == 0) {
            // Try A then B
            printf("[T%ld] Trying lock order A->B\n", tid);
            pthread_mutex_lock(&A);
            usleep(10000);
            pthread_mutex_lock(&B);
            pthread_mutex_unlock(&B);
            pthread_mutex_unlock(&A);
        } else {
            // Try B then A
            printf("[T%ld] Trying lock order B->A\n", tid);
            pthread_mutex_lock(&B);
            usleep(10000);
            pthread_mutex_lock(&A);
            pthread_mutex_unlock(&A);
            pthread_mutex_unlock(&B);
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, (void*)1);
    pthread_create(&t2, NULL, worker, (void*)2);

    // Set timeout
    sleep(3);
    printf("Test 11: DUBIOUS (random lock order - may deadlock with different timing)\n");
    return 0;
}
