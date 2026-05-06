/*
 * TEST 30: Staggered Deadlock Under Active Safe Workload
 * Category: MIXED/PARTIAL DEADLOCK
 * Description: 6 "safe" threads acquire A then B in consistent order and
 * complete many iterations. Two "dangerous" threads start with a short delay
 * so that safe threads are already running: danger1 acquires A→B (safe
 * direction) and danger2 acquires B→A (inversion), forming a deadlock pair
 * while the safe workload is still ongoing.
 * Expected: DEADLOCK detected mid-run
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* safe_worker(void* arg) {
    int tid = (int)(long)arg;
    for (int i = 0; i < 20; i++) {
        pthread_mutex_lock(&A);
        pthread_mutex_lock(&B);
        pthread_mutex_unlock(&B);
        pthread_mutex_unlock(&A);
    }
    printf("[Safe-%d] completed\n", tid);
    return NULL;
}

void* danger1(void* arg) { (void)arg;
    usleep(50000); /* let safe threads start */
    printf("[Danger1] locking A\n"); pthread_mutex_lock(&A);
    usleep(200000);
    printf("[Danger1] locking B\n"); pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A);
    return NULL; }

void* danger2(void* arg) { (void)arg;
    usleep(50000);
    printf("[Danger2] locking B\n"); pthread_mutex_lock(&B);
    usleep(200000);
    printf("[Danger2] locking A\n"); pthread_mutex_lock(&A); /* inversion */
    pthread_mutex_unlock(&A); pthread_mutex_unlock(&B);
    return NULL; }

int main(void) {
    pthread_t safe[6], d1, d2;
    for (int i = 0; i < 6; i++)
        pthread_create(&safe[i], NULL, safe_worker, (void*)(long)i);
    pthread_create(&d1, NULL, danger1, NULL);
    pthread_create(&d2, NULL, danger2, NULL);
    sleep(2);
    printf("Test 30: staggered DEADLOCK\n");
    return 1;
}
