/*
 * Description: Both threads lock in the same order (A before B).
 * This prevents circular wait, thus no deadlock.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    long tid = (long)arg;
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&A);
        usleep(5000);
        pthread_mutex_lock(&B);
        usleep(5000);
        pthread_mutex_unlock(&B);
        pthread_mutex_unlock(&A);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, (void*)1);
    pthread_create(&t2, NULL, worker, (void*)2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Test 3 PASSED: Ordered locking 2 threads completed without deadlock\n");
    return 0;
}
