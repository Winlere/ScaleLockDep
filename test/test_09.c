/*
 * TEST 9: Self-Deadlock (Recursive Lock Attempt)
 * Category: DEADLOCK
 * Description: A single thread tries to acquire the same mutex twice.
 * With a regular (non-recursive) mutex, this causes deadlock.
 * Expected: DEADLOCKS (thread blocks on itself)
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void helper_function(void) {
    printf("  [Helper] Trying to acquire lock\n");
    pthread_mutex_lock(&lock);  // Will block: lock already held by this thread!
    printf("  [Helper] Lock acquired\n");
    pthread_mutex_unlock(&lock);
}

void* worker(void* arg) {
    (void)arg;
    printf("[T1] Acquiring lock for first time\n");
    pthread_mutex_lock(&lock);
    printf("[T1] Lock acquired\n");

    usleep(100000);

    helper_function();  // This tries to lock the same mutex again!

    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void) {
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);

    sleep(1);
    printf("Test 9: SELF-DEADLOCK DETECTED (thread blocked trying to acquire its own lock)\n");
    return 1;
}
