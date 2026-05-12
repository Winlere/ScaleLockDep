/*
 * Description: Classic circular deadlock. 
 * T1: lock A -> lock B
 * T2: lock B -> lock C
 * T3: lock C -> lock A
 * Creates cycle: T1 waits for B (held by T2), T2 waits for C (held by T3), T3 waits for A (held by T1)
 * Expected: deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg) {
    (void)arg;
    pthread_mutex_lock(&A);
    usleep(200000);

    pthread_mutex_lock(&B);

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* thread2(void* arg) {
    (void)arg;
    pthread_mutex_lock(&B);
    usleep(200000);

    pthread_mutex_lock(&C);

    pthread_mutex_unlock(&C);
    pthread_mutex_unlock(&B);
    return NULL;
}

void* thread3(void* arg) {
    (void)arg;
    pthread_mutex_lock(&C);
    usleep(200000);

    pthread_mutex_lock(&A);

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
