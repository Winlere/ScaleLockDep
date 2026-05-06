/*
 * TEST 26: Dual Independent Deadlock Pairs
 * Category: DEADLOCK
 * Description: 4 threads form two simultaneous, independent 2-thread
 * deadlocks. T1 and T2 deadlock over {A, B}; T3 and T4 deadlock over
 * {C, D}. The two cycles share no locks and arise concurrently, testing
 * the detector's ability to report multiple live deadlock cycles at once.
 * Expected: DEADLOCKS
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;

void* t1(void* arg) { (void)arg;
    printf("[T1] locking A\n"); pthread_mutex_lock(&A);
    usleep(200000);
    printf("[T1] locking B\n"); pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A); return NULL; }

void* t2(void* arg) { (void)arg;
    printf("[T2] locking B\n"); pthread_mutex_lock(&B);
    usleep(200000);
    printf("[T2] locking A\n"); pthread_mutex_lock(&A);
    pthread_mutex_unlock(&A); pthread_mutex_unlock(&B); return NULL; }

void* t3(void* arg) { (void)arg;
    printf("[T3] locking C\n"); pthread_mutex_lock(&C);
    usleep(200000);
    printf("[T3] locking D\n"); pthread_mutex_lock(&D);
    pthread_mutex_unlock(&D); pthread_mutex_unlock(&C); return NULL; }

void* t4(void* arg) { (void)arg;
    printf("[T4] locking D\n"); pthread_mutex_lock(&D);
    usleep(200000);
    printf("[T4] locking C\n"); pthread_mutex_lock(&C);
    pthread_mutex_unlock(&C); pthread_mutex_unlock(&D); return NULL; }

int main(void) {
    pthread_t threads[4];
    pthread_create(&threads[0], NULL, t1, NULL);
    pthread_create(&threads[1], NULL, t2, NULL);
    pthread_create(&threads[2], NULL, t3, NULL);
    pthread_create(&threads[3], NULL, t4, NULL);
    sleep(2);
    printf("Test 26: dual-pair DEADLOCK\n");
    return 1;
}
