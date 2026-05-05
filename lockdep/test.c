#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

static void *worker1(void *arg) {
    (void)arg;
    pthread_mutex_lock(&A);
    pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

static void *worker2(void *arg) {
    (void)arg;
    pthread_mutex_lock(&B);
    pthread_mutex_lock(&A);
    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void) {
    pthread_t t;

    pthread_create(&t, NULL, worker1, NULL);
    pthread_join(t, NULL);

    pthread_create(&t, NULL, worker2, NULL);
    pthread_join(t, NULL);

    puts("done");
    return 0;
}
