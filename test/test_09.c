/*
 * Description: A single thread tries to acquire the same mutex twice.
 * With a regular (non-recursive) mutex, this causes deadlock.
 * Expected: deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void helper_function(void) {
    pthread_mutex_lock(&lock);  // Will block: lock already held by this thread!
    pthread_mutex_unlock(&lock);
}

void* worker(void* arg) {
    (void)arg;
    pthread_mutex_lock(&lock);

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
