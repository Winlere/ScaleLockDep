/*
 * Description: Thread 1 locks A then B. Thread 2 locks B then A.
 * Creates circular wait: T1 waits for B (held by T2), T2 waits for A (held by T1).
 * Expected: deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg) {
    (void)arg;
    pthread_mutex_lock(&A);

    usleep(200000);

    pthread_mutex_lock(&B);  // Will wait here if T2 has B

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* thread2(void* arg) {
    (void)arg;
    pthread_mutex_lock(&B);

    usleep(200000);

    pthread_mutex_lock(&A);  // Will wait here if T1 has A

    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);

    // Add timeout mechanism for test suite
    sleep(2);
    printf("Test 7: DEADLOCK DETECTED (program should have finished by now)\n");
    return 1;  // Indicate deadlock (failure)
}
