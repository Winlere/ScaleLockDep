/*
 * TEST 16: Asymmetric Deadlock Pattern
 * Category: MIXED DEADLOCK
 * Description: Three threads where pairs are safe, but all three together deadlock.
 * T1 and T2 can safely run (they use A and B in same order)
 * T1 and T3 can safely run (they use different locks)
 * But T1, T2, T3 together form deadlock cycle.
 * T1: lock A -> lock B
 * T2: lock B -> lock A (deadlock with T1)
 * T3: lock B -> lock C (partial interaction)
 * Expected: DEADLOCKS due to cycle between T1 and T2
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

void* worker1(void* arg) {
    (void)arg;
    printf("[T1] locking A\n");
    pthread_mutex_lock(&A);
    printf("[T1] locked A\n");
    usleep(150000);

    printf("[T1] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T1] locked B\n");

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    printf("[T1] done\n");
    return NULL;
}

void* worker2(void* arg) {
    (void)arg;
    printf("[T2] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T2] locked B\n");
    usleep(150000);

    printf("[T2] locking A\n");
    pthread_mutex_lock(&A);  // Deadlock with T1
    printf("[T2] locked A\n");

    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    printf("[T2] done\n");
    return NULL;
}

void* worker3(void* arg) {
    (void)arg;
    printf("[T3] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T3] locked B\n");
    usleep(150000);

    printf("[T3] locking C\n");
    pthread_mutex_lock(&C);
    printf("[T3] locked C\n");

    pthread_mutex_unlock(&C);
    pthread_mutex_unlock(&B);
    printf("[T3] done\n");
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, worker1, NULL);
    pthread_create(&t2, NULL, worker2, NULL);
    pthread_create(&t3, NULL, worker3, NULL);

    sleep(2);
    printf("\nTest 16: ASYMMETRIC DEADLOCK\n");
    printf("  Threads 1 and 2: DEADLOCK (t1 waits B held by t2, t2 waits A held by t1)\n");
    printf("  Thread 3: May block or proceed depending on timing\n");
    return 1;
}
