#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg) {
    (void)arg;
    printf("[T1] locking A\n");
    pthread_mutex_lock(&A);
    printf("[T1] locked A\n");

    usleep(200000);

    printf("[T1] locking B\n");
    pthread_mutex_lock(&B);
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
    pthread_mutex_lock(&A);
    printf("[T2] locked A\n");

    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Done\n");
    return 0;
}