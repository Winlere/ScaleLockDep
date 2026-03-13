#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

static const int NUM_THREADS = 40;
static const int ITERS = 10000;

void* worker(void* arg) {
    long tid = (long)arg;

    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&A);
        pthread_mutex_lock(&B);
        pthread_mutex_lock(&C);

        if (i % 3000 == 0) {
            printf("[T%ld] progress %d\n", tid, i);
        }

        pthread_mutex_unlock(&C);
        pthread_mutex_unlock(&B);
        pthread_mutex_unlock(&A);
    }

    return NULL;
}

int main(void) {
    pthread_t th[NUM_THREADS];

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&th[i], NULL, worker, (void*)i);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(th[i], NULL);
    }

    printf("Ordered locking finished without deadlock.\n");
    return 0;
}