/*
 * TEST 14: Partial Deadlock - Some Threads Blocked, Some Free
 * Category: MIXED/PARTIAL DEADLOCK
 * Description: 4 threads total. Thread 1 and 2 form a deadlock (A->B vs B->A).
 * Threads 3 and 4 use independent locks and run freely.
 * Expected: Threads 3 and 4 complete, threads 1 and 2 deadlock
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;
static int progress[4] = {0, 0, 0, 0};

void* deadlock_worker1(void* arg) {
    printf("[T1] Starting (will deadlock)\n");
    pthread_mutex_lock(&A);
    printf("[T1] Locked A\n");
    usleep(200000);
    printf("[T1] Trying to lock B\n");
    pthread_mutex_lock(&B);  // Will wait forever (T2 has B)
    printf("[T1] Locked B\n");
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* deadlock_worker2(void* arg) {
    printf("[T2] Starting (will deadlock)\n");
    pthread_mutex_lock(&B);
    printf("[T2] Locked B\n");
    usleep(200000);
    printf("[T2] Trying to lock A\n");
    pthread_mutex_lock(&A);  // Will wait forever (T1 has A)
    printf("[T2] Locked A\n");
    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

void* free_worker3(void* arg) {
    printf("[T3] Starting (will complete freely)\n");
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&C);
        progress[2]++;
        pthread_mutex_unlock(&C);
        if (i % 10 == 0) printf("[T3] Progress: %d\n", progress[2]);
    }
    printf("[T3] Completed successfully\n");
    return NULL;
}

void* free_worker4(void* arg) {
    printf("[T4] Starting (will complete freely)\n");
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&D);
        progress[3]++;
        pthread_mutex_unlock(&D);
        if (i % 10 == 0) printf("[T4] Progress: %d\n", progress[3]);
    }
    printf("[T4] Completed successfully\n");
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, deadlock_worker1, NULL);
    pthread_create(&t2, NULL, deadlock_worker2, NULL);
    pthread_create(&t3, NULL, free_worker3, NULL);
    pthread_create(&t4, NULL, free_worker4, NULL);

    sleep(2);
    printf("\nTest 14: PARTIAL DEADLOCK\n");
    printf("  Threads 1 and 2: DEADLOCKED\n");
    printf("  Thread 3: Completed (progress=%d)\n", progress[2]);
    printf("  Thread 4: Completed (progress=%d)\n", progress[3]);
    return 1;
}
