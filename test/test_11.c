/*
 * TEST 11: Dubious Round-Robin Lock Order
 * Category: DUBIOUS
 * Description: Each iteration rotates the lock acquisition order to "balance
 *   load" across locks — a pattern that looks like a smart optimization to
 *   reduce hot-lock contention.  A reader might think the alternating scheme
 *   is safe because both threads do the same rotation in lock-step, but the
 *   two orderings (A→B on even iterations, B→A on odd) constitute conflicting
 *   edges in the lock graph.  A deadlock detector sees the cycle immediately.
 * Expected: DEADLOCK detected (conflicting A→B / B→A orderings are registered)
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    long tid = (long)arg;

    for (int i = 0; i < 20; i++) {
        /* Rotate order each iteration — "reduces hot-lock contention" */
        if (i % 2 == 0) {
            printf("[T%ld i=%d] A->B\n", tid, i);
            pthread_mutex_lock(&A);
            usleep(5000);
            pthread_mutex_lock(&B);
            pthread_mutex_unlock(&B);
            pthread_mutex_unlock(&A);
        } else {
            printf("[T%ld i=%d] B->A\n", tid, i);
            pthread_mutex_lock(&B);
            usleep(5000);
            pthread_mutex_lock(&A);
            pthread_mutex_unlock(&A);
            pthread_mutex_unlock(&B);
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, (void*)1L);
    pthread_create(&t2, NULL, worker, (void*)2L);
    /* Lockdep exits early (rc=66) on detecting the conflicting orderings */
    sleep(3);
    printf("Test 11: completed without deadlock detection\n");
    return 0;
}
