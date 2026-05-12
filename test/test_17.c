/*
 * Description: 10 threads compete for a single mutex, each acquiring it 200
 * times. A single lock cannot create a circular wait (no "hold-and-wait"
 * between multiple resources), so deadlock is impossible by Coffman's
 * conditions. Tests detector accuracy under high lock-contention load.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 10
#define ITERATIONS  200

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int counter = 0;

void* worker(void* arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void*)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("Test 17 PASSED: lock convoy completed, counter=%d\n", counter);
    return 0;
}
