/*
 * Description: Uses pthread_mutex_trylock to avoid blocking.
 * If try fails, it releases and retries. Avoids standard deadlock
 * but can lead to livelock or starvation under high contention.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    long tid = (long)arg;
    int acquired_both = 0;

    for (int i = 0; i < 30 && !acquired_both; i++) {
        int retries = 0;
        acquired_both = 0;

        while (!acquired_both && retries < 100) {
            if (pthread_mutex_trylock(&A) == 0) {
                // Got A, try to get B
                if (pthread_mutex_trylock(&B) == 0) {
                    // Got both!
                    acquired_both = 1;
                    pthread_mutex_unlock(&B);
                    pthread_mutex_unlock(&A);
                } else {
                    // Failed to get B, release A and retry
                    pthread_mutex_unlock(&A);
                    retries++;
                    usleep(1000);
                }
            } else {
                // Failed to get A, retry
                usleep(1000);
                retries++;
            }
        }

        if (!acquired_both) {
        }
    }

    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, (void*)1);
    pthread_create(&t2, NULL, worker, (void*)2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Test 12 PASSED: Try-lock completed (avoids deadlock but risky)\n");
    return 0;
}
