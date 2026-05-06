/*
 * TEST 29: Triple-Pair Mixed Scenario
 * Category: MIXED/PARTIAL DEADLOCK
 * Description: 6 threads operate in 3 independent pairs. Pair 1 (T1, T2)
 * deadlocks on {A, B} (A↔B inversion); pair 2 (T3, T4) deadlocks on {C, D}
 * (C↔D inversion); pair 3 (T5, T6) safely acquires E then F in consistent
 * order and completes freely. Two simultaneous deadlocks coexist with active
 * safe threads.
 * Expected: DEADLOCK detected; T5 and T6 would complete
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t E = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t F = PTHREAD_MUTEX_INITIALIZER;

void* t1(void* arg) { (void)arg;
    pthread_mutex_lock(&A); usleep(200000); pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A); return NULL; }

void* t2(void* arg) { (void)arg;
    pthread_mutex_lock(&B); usleep(200000); pthread_mutex_lock(&A);
    pthread_mutex_unlock(&A); pthread_mutex_unlock(&B); return NULL; }

void* t3(void* arg) { (void)arg;
    pthread_mutex_lock(&C); usleep(200000); pthread_mutex_lock(&D);
    pthread_mutex_unlock(&D); pthread_mutex_unlock(&C); return NULL; }

void* t4(void* arg) { (void)arg;
    pthread_mutex_lock(&D); usleep(200000); pthread_mutex_lock(&C);
    pthread_mutex_unlock(&C); pthread_mutex_unlock(&D); return NULL; }

void* t5(void* arg) { (void)arg;
    for (int i = 0; i < 30; i++) {
        pthread_mutex_lock(&E);
        pthread_mutex_lock(&F);
        pthread_mutex_unlock(&F);
        pthread_mutex_unlock(&E);
    }
    printf("[T5] Completed safely\n");
    return NULL; }

void* t6(void* arg) { (void)arg;
    for (int i = 0; i < 30; i++) {
        pthread_mutex_lock(&E);
        pthread_mutex_lock(&F);
        pthread_mutex_unlock(&F);
        pthread_mutex_unlock(&E);
    }
    printf("[T6] Completed safely\n");
    return NULL; }

int main(void) {
    pthread_t threads[6];
    pthread_create(&threads[0], NULL, t1, NULL);
    pthread_create(&threads[1], NULL, t2, NULL);
    pthread_create(&threads[2], NULL, t3, NULL);
    pthread_create(&threads[3], NULL, t4, NULL);
    pthread_create(&threads[4], NULL, t5, NULL);
    pthread_create(&threads[5], NULL, t6, NULL);
    sleep(2);
    printf("Test 29: triple-pair PARTIAL DEADLOCK\n");
    return 1;
}
