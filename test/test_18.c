/*
 * Description: 2 producer threads enqueue items into a bounded buffer; 2
 * consumer threads dequeue them using a single mutex + pthread_cond_wait.
 * pthread_cond_wait releases the mutex atomically, so no thread ever holds
 * two locks simultaneously and no circular dependency can form.
 * Expected: no deadlock detected
 */
#include <pthread.h>
#include <stdio.h>

#define QUEUE_SIZE   8
#define ITEMS_EACH  20   /* each producer enqueues this many items */
#define TOTAL_ITEMS (2 * ITEMS_EACH)

static pthread_mutex_t mu       = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  not_full  = PTHREAD_COND_INITIALIZER;

static int queue[QUEUE_SIZE];
static int head = 0, tail = 0, count = 0;
static int done = 0; /* set after all producers finish */

void* producer(void* arg) {
    int id = (int)(long)arg;
    for (int i = 0; i < ITEMS_EACH; i++) {
        pthread_mutex_lock(&mu);
        while (count == QUEUE_SIZE)
            pthread_cond_wait(&not_full, &mu);
        queue[tail] = id * 100 + i;
        tail = (tail + 1) % QUEUE_SIZE;
        count++;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}

void* consumer(void* arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&mu);
        while (count == 0 && !done)
            pthread_cond_wait(&not_empty, &mu);
        if (count == 0 && done) {
            pthread_mutex_unlock(&mu);
            break;
        }
        head = (head + 1) % QUEUE_SIZE;
        count--;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}

int main(void) {
    pthread_t p1, p2, c1, c2;
    pthread_create(&c1, NULL, consumer, NULL);
    pthread_create(&c2, NULL, consumer, NULL);
    pthread_create(&p1, NULL, producer, (void*)1L);
    pthread_create(&p2, NULL, producer, (void*)2L);
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    pthread_mutex_lock(&mu);
    done = 1;
    pthread_cond_broadcast(&not_empty);
    pthread_mutex_unlock(&mu);
    pthread_join(c1, NULL);
    pthread_join(c2, NULL);
    printf("Test 18 PASSED: producer-consumer with condvar completed\n");
    return 0;
}
