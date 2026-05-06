/*
 * TEST 23: 5-Thread Pentagon Deadlock
 * Category: DEADLOCK
 * Description: 5 threads form a circular wait chain of length 5: T1 holds A
 * and waits for B, T2 holds B and waits for C, T3 holds C and waits for D,
 * T4 holds D and waits for E, T5 holds E and waits for A. All 5 are
 * simultaneously blocked, constituting a pentagon-shaped deadlock cycle.
 * Expected: DEADLOCKS
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t E = PTHREAD_MUTEX_INITIALIZER;

#define DELAY 200000

void* t1(void* a) { (void)a;
    printf("[T1] locking A\n"); pthread_mutex_lock(&A);
    usleep(DELAY);
    printf("[T1] locking B\n"); pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A); return NULL; }

void* t2(void* a) { (void)a;
    printf("[T2] locking B\n"); pthread_mutex_lock(&B);
    usleep(DELAY);
    printf("[T2] locking C\n"); pthread_mutex_lock(&C);
    pthread_mutex_unlock(&C); pthread_mutex_unlock(&B); return NULL; }

void* t3(void* a) { (void)a;
    printf("[T3] locking C\n"); pthread_mutex_lock(&C);
    usleep(DELAY);
    printf("[T3] locking D\n"); pthread_mutex_lock(&D);
    pthread_mutex_unlock(&D); pthread_mutex_unlock(&C); return NULL; }

void* t4(void* a) { (void)a;
    printf("[T4] locking D\n"); pthread_mutex_lock(&D);
    usleep(DELAY);
    printf("[T4] locking E\n"); pthread_mutex_lock(&E);
    pthread_mutex_unlock(&E); pthread_mutex_unlock(&D); return NULL; }

void* t5(void* a) { (void)a;
    printf("[T5] locking E\n"); pthread_mutex_lock(&E);
    usleep(DELAY);
    printf("[T5] locking A\n"); pthread_mutex_lock(&A); /* closes the cycle */
    pthread_mutex_unlock(&A); pthread_mutex_unlock(&E); return NULL; }

int main(void) {
    pthread_t threads[5];
    pthread_create(&threads[0], NULL, t1, NULL);
    pthread_create(&threads[1], NULL, t2, NULL);
    pthread_create(&threads[2], NULL, t3, NULL);
    pthread_create(&threads[3], NULL, t4, NULL);
    pthread_create(&threads[4], NULL, t5, NULL);
    sleep(2);
    printf("Test 23: 5-thread pentagon DEADLOCK\n");
    return 1;
}
