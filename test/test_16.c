/*
 * Description: Three threads where pairs are safe, but all three together deadlock.
 * T1 and T2 can safely run (they use A and B in same order)
 * T1 and T3 can safely run (they use different locks)
 * But T1, T2, T3 together form deadlock cycle.
 * T1: lock A -> lock B
 * T2: lock B -> lock A (deadlock with T1)
 * T3: lock B -> lock C (partial interaction)
 * Expected: deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

void* worker1(void* arg) {
    (void)arg;
    pthread_mutex_lock(&A);
    usleep(150000);

    pthread_mutex_lock(&B);

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* worker2(void* arg) {
    (void)arg;
    pthread_mutex_lock(&B);
    usleep(150000);

    pthread_mutex_lock(&A);  // Deadlock with T1

    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

void* worker3(void* arg) {
    (void)arg;
    pthread_mutex_lock(&B);
    usleep(150000);

    pthread_mutex_lock(&C);

    pthread_mutex_unlock(&C);
    pthread_mutex_unlock(&B);
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
