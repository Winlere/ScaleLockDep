/*
 * Description: Models the lock-layer inversion bug class documented by
 * Engler & Ashcraft (RacerX, SOSP 2003). A correct thread acquires the
 * high-level lock H before the low-level lock L; a second thread acquires
 * them in the opposite order (L then H), creating a cycle H→L→H.
 * Expected: deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t H = PTHREAD_MUTEX_INITIALIZER; /* high-level lock */
static pthread_mutex_t L = PTHREAD_MUTEX_INITIALIZER; /* low-level lock  */

void* correct_layer(void* arg) {
    (void)arg;
    pthread_mutex_lock(&H);
    usleep(200000);
    pthread_mutex_lock(&L);  /* waits if T2 already holds L */
    pthread_mutex_unlock(&L);
    pthread_mutex_unlock(&H);
    return NULL;
}

void* inverted_layer(void* arg) {
    (void)arg;
    pthread_mutex_lock(&L);
    usleep(200000);
    pthread_mutex_lock(&H);  /* waits if T1 already holds H */
    pthread_mutex_unlock(&H);
    pthread_mutex_unlock(&L);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, correct_layer,  NULL);
    pthread_create(&t2, NULL, inverted_layer, NULL);
    sleep(2);
    printf("Test 24: layer-inversion DEADLOCK\n");
    return 1;
}
