/*
 * TEST 7: Classic 2-Thread, 2-Lock Deadlock
 * Category: DEADLOCK
 * Description: Thread 1 locks A then B. Thread 2 locks B then A.
 * Creates circular wait: T1 waits for B (held by T2), T2 waits for A (held by T1).
 * Expected: DEADLOCKS (or hangs indefinitely)
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg) {
    (void)arg;
    printf("[T1] locking A\n");
    pthread_mutex_lock(&A);
    printf("[T1] locked A\n");

    usleep(200000);

    printf("[T1] locking B\n");
    pthread_mutex_lock(&B);  // Will wait here if T2 has B
    printf("[T1] locked B\n");

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

void* thread2(void* arg) {
    (void)arg;
    printf("[T2] locking B\n");
    pthread_mutex_lock(&B);
    printf("[T2] locked B\n");

    usleep(200000);

    printf("[T2] locking A\n");
    pthread_mutex_lock(&A);  // Will wait here if T1 has A
    printf("[T2] locked A\n");

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
