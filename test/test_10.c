/*
 * TEST 10: 4-Thread Circular Deadlock
 * Category: DEADLOCK
 * Description: Four threads in a cycle.
 * T1: lock A -> lock B
 * T2: lock B -> lock C
 * T3: lock C -> lock D
 * T4: lock D -> lock A
 * Cycle: T1 waits for B (T2), T2 waits for C (T3), T3 waits for D (T4), T4 waits for A (T1)
 * Expected: DEADLOCKS
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg) {
    (void)arg;
    printf("[T1] locking A\n");
    pthread_mutex_lock(&A);
    usleep(100000);
    printf("[T1] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T1] done\n");
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* thread2(void* arg) {
    (void)arg;
    printf("[T2] locking B\n");
    pthread_mutex_lock(&B);
    usleep(100000);
    printf("[T2] locking C\n");
    pthread_mutex_lock(&C);
    printf("[T2] done\n");
    pthread_mutex_unlock(&C);
    pthread_mutex_unlock(&B);
    return NULL;
}

void* thread3(void* arg) {
    (void)arg;
    printf("[T3] locking C\n");
    pthread_mutex_lock(&C);
    usleep(100000);
    printf("[T3] locking D\n");
    pthread_mutex_lock(&D);
    printf("[T3] done\n");
    pthread_mutex_unlock(&D);
    pthread_mutex_unlock(&C);
    return NULL;
}

void* thread4(void* arg) {
    (void)arg;
    printf("[T4] locking D\n");
    pthread_mutex_lock(&D);
    usleep(100000);
    printf("[T4] locking A\n");
    pthread_mutex_lock(&A);
    printf("[T4] done\n");
    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&D);
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);
    pthread_create(&t3, NULL, thread3, NULL);
    pthread_create(&t4, NULL, thread4, NULL);

    sleep(2);
    printf("Test 10: DEADLOCK DETECTED (4-thread circular deadlock)\n");
    return 1;
}
