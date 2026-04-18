/*
 * TEST 8: Circular 3-Thread Deadlock
 * Category: DEADLOCK
 * Description: Classic circular deadlock. 
 * T1: lock A -> lock B
 * T2: lock B -> lock C
 * T3: lock C -> lock A
 * Creates cycle: T1 waits for B (held by T2), T2 waits for C (held by T3), T3 waits for A (held by T1)
 * Expected: DEADLOCKS
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg) {
    (void)arg;
    printf("[T1] locking A\n");
    pthread_mutex_lock(&A);
    printf("[T1] locked A\n");
    usleep(200000);

    printf("[T1] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T1] locked B\n");

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* thread2(void* arg) {
    (void)arg;
    printf("[T2] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T2] locked B\n");
    usleep(200000);

    printf("[T2] locking C\n");
    pthread_mutex_lock(&C);
    printf("[T2] locked C\n");

    pthread_mutex_unlock(&C);
    pthread_mutex_unlock(&B);
    return NULL;
}

void* thread3(void* arg) {
    (void)arg;
    printf("[T3] locking C\n");
    pthread_mutex_lock(&C);
    printf("[T3] locked C\n");
    usleep(200000);

    printf("[T3] locking A\n");
    pthread_mutex_lock(&A);
    printf("[T3] locked A\n");

    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&C);
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);
    pthread_create(&t3, NULL, thread3, NULL);

    sleep(2);
    printf("Test 8: DEADLOCK DETECTED (circular 3-thread deadlock)\n");
    return 1;
}
