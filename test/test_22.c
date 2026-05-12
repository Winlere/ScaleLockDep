/*
 * Description: 4 reader threads use pthread_mutex_trylock with a short
 * nanosleep backoff; 1 writer uses blocking pthread_mutex_lock. No thread
 * ever nests multiple locks, so the dependency graph never acquires any
 * edges and no cycle is possible despite the mixed trylock/blocking access.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NUM_READERS  4
#define READER_ITERS 50
#define WRITER_ITERS 20

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int shared = 0;

void* reader(void* arg) {
    (void)arg;
    for (int i = 0; i < READER_ITERS; i++) {
        while (pthread_mutex_trylock(&lock) != 0) {
            struct timespec ts = {0, 100000L}; /* 0.1 ms backoff */
            nanosleep(&ts, NULL);
        }
        int val = shared;
        pthread_mutex_unlock(&lock);
        (void)val;
    }
    return NULL;
}

void* writer(void* arg) {
    (void)arg;
    for (int i = 0; i < WRITER_ITERS; i++) {
        pthread_mutex_lock(&lock);
        shared++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(void) {
    pthread_t readers[NUM_READERS], w;
    for (int i = 0; i < NUM_READERS; i++)
        pthread_create(&readers[i], NULL, reader, (void*)(long)i);
    pthread_create(&w, NULL, writer, NULL);
    for (int i = 0; i < NUM_READERS; i++)
        pthread_join(readers[i], NULL);
    pthread_join(w, NULL);
    printf("Test 22 PASSED: trylock readers + writer completed, shared=%d\n",
           shared);
    return 0;
}
