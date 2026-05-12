/*
 * Description: 4 threads each cycle through 3 locks sequentially, fully
 * releasing each before acquiring the next. Because no thread ever holds
 * more than one lock at a time, the "hold-and-wait" Coffman condition is
 * never satisfied and circular wait is structurally impossible.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 4
#define ITERATIONS  100

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&A);
        pthread_mutex_unlock(&A);

        pthread_mutex_lock(&B);
        pthread_mutex_unlock(&B);

        pthread_mutex_lock(&C);
        pthread_mutex_unlock(&C);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void*)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("Test 21 PASSED: sequential non-nested locking completed\n");
    return 0;
}
