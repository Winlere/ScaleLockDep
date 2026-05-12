/*
 * Description: 4 threads form two simultaneous, independent 2-thread
 * deadlocks. T1 and T2 deadlock over {A, B}; T3 and T4 deadlock over
 * {C, D}. The two cycles share no locks and arise concurrently, testing
 * the detector's ability to report multiple live deadlock cycles at once.
 * Expected: deadlock detected
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;

void* t1(void* arg) { (void)arg;
    usleep(200000);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A); return NULL; }

void* t2(void* arg) { (void)arg;
    usleep(200000);
    pthread_mutex_unlock(&A); pthread_mutex_unlock(&B); return NULL; }

void* t3(void* arg) { (void)arg;
    usleep(200000);
    pthread_mutex_unlock(&D); pthread_mutex_unlock(&C); return NULL; }

void* t4(void* arg) { (void)arg;
    usleep(200000);
    pthread_mutex_unlock(&C); pthread_mutex_unlock(&D); return NULL; }

int main(void) {
    pthread_t threads[4];
    pthread_create(&threads[0], NULL, t1, NULL);
    pthread_create(&threads[1], NULL, t2, NULL);
    pthread_create(&threads[2], NULL, t3, NULL);
    pthread_create(&threads[3], NULL, t4, NULL);
    sleep(2);
    printf("Test 26: dual-pair DEADLOCK\n");
    return 1;
}
