/*
 * Description: Multiple threads hit a barrier. Some proceed normally,
 * while a subset engage in deadlock-forming behavior after the barrier.
 * Expected: deadlock detected
 */
#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static pthread_barrier_t barrier;
static pthread_mutex_t X = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t Y = PTHREAD_MUTEX_INITIALIZER;
static int completed[4] = {0, 0, 0, 0};

void* normal_worker(void* arg) {
    long tid = (long)arg;
    pthread_barrier_wait(&barrier);

    for (int i = 0; i < 20; i++) {
        pthread_mutex_lock(&X);
        completed[tid] = i;
        pthread_mutex_unlock(&X);
    }
    return NULL;
}

void* deadlock_worker1(void* arg) {
    long tid = (long)arg;
    pthread_barrier_wait(&barrier);

    pthread_mutex_lock(&X);
    usleep(100000);
    pthread_mutex_lock(&Y);
    pthread_mutex_unlock(&Y);
    pthread_mutex_unlock(&X);
    return NULL;
}

void* deadlock_worker2(void* arg) {
    long tid = (long)arg;
    pthread_barrier_wait(&barrier);

    pthread_mutex_lock(&Y);
    usleep(100000);
    pthread_mutex_lock(&X);
    pthread_mutex_unlock(&X);
    pthread_mutex_unlock(&Y);
    return NULL;
}

int main(void) {
    pthread_barrier_init(&barrier, NULL, 4);

    pthread_t t0, t1, t2, t3;
    pthread_create(&t0, NULL, normal_worker, (void*)0);
    pthread_create(&t1, NULL, normal_worker, (void*)1);
    pthread_create(&t2, NULL, deadlock_worker1, (void*)2);
    pthread_create(&t3, NULL, deadlock_worker2, (void*)3);

    sleep(2);
    printf("\nTest 15: BARRIER WITH PARTIAL DEADLOCK\n");
    printf("  Threads 0,1: Progressed normally\n");
    printf("  Threads 2,3: DEADLOCKED after barrier\n");
    return 1;
}
