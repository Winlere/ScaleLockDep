#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_LOCKS   40
#define NUM_THREADS 40
#define ITERS       10000

static pthread_mutex_t locks[NUM_LOCKS];

void* worker(void* arg) {
    long tid = (long)arg;

    for (int i = 0; i < ITERS; i++) {
        for (int j = 0; j < NUM_LOCKS; j++)
            pthread_mutex_lock(&locks[j]);

        if (i % 3000 == 0) {
            printf("[T%ld] progress %d\n", tid, i);
        }

        for (int j = NUM_LOCKS - 1; j >= 0; j--)
            pthread_mutex_unlock(&locks[j]);
    }

    return NULL;
}

int main(void) {
    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_init(&locks[i], NULL);

    pthread_t th[NUM_THREADS];

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&th[i], NULL, worker, (void*)i);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(th[i], NULL);
    }

    for (int i = 0; i < NUM_LOCKS; i++)
        pthread_mutex_destroy(&locks[i]);

    printf("Ordered locking finished without deadlock.\n");
    return 0;
}