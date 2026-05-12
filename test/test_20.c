/*
 * Description: 8 threads each hold a private local mutex for per-thread work,
 * then acquire a single shared global mutex to update a counter. All threads
 * follow the same local[i]→global hierarchy, so the dependency graph is a
 * DAG (star topology) with no cycle possible.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 8
#define ITERATIONS  50

static pthread_mutex_t local_locks[NUM_THREADS];
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
static int global_counter = 0;

void* worker(void* arg) {
    int tid = (int)(long)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&local_locks[tid]);
        pthread_mutex_lock(&global_lock);
        global_counter++;
        pthread_mutex_unlock(&global_lock);
        pthread_mutex_unlock(&local_locks[tid]);
    }
    return NULL;
}

int main(void) {
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_mutex_init(&local_locks[i], NULL);
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void*)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("Test 20 PASSED: per-thread local+global locking, counter=%d\n",
           global_counter);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_mutex_destroy(&local_locks[i]);
    return 0;
}
