/*
 * Description: Two threads with completely disjoint lock sets. Thread 1 uses locks A and B,
 * Thread 2 uses locks C and D. No lock sharing, so no deadlock.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;

void* worker1(void* arg) {
    (void)arg;
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&A);
        usleep(2000);
        pthread_mutex_lock(&B);
        pthread_mutex_unlock(&B);
        pthread_mutex_unlock(&A);
    }
    return NULL;
}

void* worker2(void* arg) {
    (void)arg;
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&C);
        usleep(2000);
        pthread_mutex_lock(&D);
        pthread_mutex_unlock(&D);
        pthread_mutex_unlock(&C);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker1, NULL);
    pthread_create(&t2, NULL, worker2, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Test 5 PASSED: Disjoint locks completed without deadlock\n");
    return 0;
}
