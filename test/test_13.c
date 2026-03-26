/*
 * TEST 13: Dubious Role-Based Lock Order
 * Category: DUBIOUS
 * Description: Each thread is assigned a "role" by its ID (mod 3), giving it a
 *   fixed pair of locks to acquire.  The code looks disciplined — no randomness,
 *   each thread has a clear, predictable responsibility.  A reviewer might reason
 *   "every thread has a consistent order, so there can be no cycle."  But the
 *   three roles collectively form a closed cycle: A→B (role 0), B→C (role 1),
 *   C→A (role 2).  The graph-level cycle is unambiguous; lockdep will catch it.
 * Expected: DEADLOCK detected (A→B→C→A cycle in the lock graph)
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
        /* Role assigned by thread ID — looks like a clean total order */
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
    pthread_create(&t1, NULL, worker, (void*)0L);
    pthread_create(&t2, NULL, worker, (void*)1L);
    pthread_create(&t3, NULL, worker, (void*)2L);
    /* Lockdep exits early (rc=66) when it detects the A->B->C->A cycle */
    sleep(2);
    printf("Test 13: completed without deadlock detection\n");
    return 0;
}
