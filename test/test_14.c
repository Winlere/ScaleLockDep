/*
 * Description: 4 threads total. Thread 1 and 2 form a deadlock (A->B vs B->A).
 * Threads 3 and 4 use independent locks and run freely.
 * Expected: deadlock detected
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
    pthread_mutex_lock(&A);
    usleep(200000);
    pthread_mutex_lock(&B);  // Will wait forever (T2 has B)
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* deadlock_worker2(void* arg) {
    pthread_mutex_lock(&B);
    usleep(200000);
    pthread_mutex_lock(&A);  // Will wait forever (T1 has A)
    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

void* free_worker3(void* arg) {
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&C);
        progress[2]++;
        pthread_mutex_unlock(&C);
    }
    return NULL;
}

void* free_worker4(void* arg) {
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&D);
        progress[3]++;
        pthread_mutex_unlock(&D);
    }
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
