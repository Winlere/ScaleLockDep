/*
 * Description: 16 threads each acquire 4 locks in the same fixed order
 * A→B→C→D. Consistent lock ordering is the canonical prevention strategy
 * for circular wait (Coffman et al. 1971). Scales test_04 to verify the
 * detector's correctness under a larger thread count.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 16
#define ITERATIONS  60

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&A);
        pthread_mutex_lock(&B);
        pthread_mutex_lock(&C);
        pthread_mutex_lock(&D);
        pthread_mutex_unlock(&D);
        pthread_mutex_unlock(&C);
        pthread_mutex_unlock(&B);
        pthread_mutex_unlock(&A);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void*)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("Test 19 PASSED: 16 threads ordered A->B->C->D completed\n");
    return 0;
}
