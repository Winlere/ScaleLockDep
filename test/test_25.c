/*
 * TEST 25: 6-Thread Hexagon Deadlock
 * Category: DEADLOCK
 * Description: 6 threads form a circular wait of length 6: T1:A→B, T2:B→C,
 * T3:C→D, T4:D→E, T5:E→F, T6:F→A. Each thread acquires its first lock and
 * sleeps before trying its second, ensuring all six hold their first lock
 * simultaneously and then block — a hexagonal deadlock cycle.
 * Expected: DEADLOCKS
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t C = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t D = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t E = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t F = PTHREAD_MUTEX_INITIALIZER;

#define DELAY 200000

void* t1(void* a){(void)a; pthread_mutex_lock(&A); usleep(DELAY); pthread_mutex_lock(&B); pthread_mutex_unlock(&B); pthread_mutex_unlock(&A); return NULL;}
void* t2(void* a){(void)a; pthread_mutex_lock(&B); usleep(DELAY); pthread_mutex_lock(&C); pthread_mutex_unlock(&C); pthread_mutex_unlock(&B); return NULL;}
void* t3(void* a){(void)a; pthread_mutex_lock(&C); usleep(DELAY); pthread_mutex_lock(&D); pthread_mutex_unlock(&D); pthread_mutex_unlock(&C); return NULL;}
void* t4(void* a){(void)a; pthread_mutex_lock(&D); usleep(DELAY); pthread_mutex_lock(&E); pthread_mutex_unlock(&E); pthread_mutex_unlock(&D); return NULL;}
void* t5(void* a){(void)a; pthread_mutex_lock(&E); usleep(DELAY); pthread_mutex_lock(&F); pthread_mutex_unlock(&F); pthread_mutex_unlock(&E); return NULL;}
void* t6(void* a){(void)a; pthread_mutex_lock(&F); usleep(DELAY); pthread_mutex_lock(&A); pthread_mutex_unlock(&A); pthread_mutex_unlock(&F); return NULL;}

int main(void) {
    pthread_t threads[6];
    pthread_create(&threads[0], NULL, t1, NULL);
    pthread_create(&threads[1], NULL, t2, NULL);
    pthread_create(&threads[2], NULL, t3, NULL);
    pthread_create(&threads[3], NULL, t4, NULL);
    pthread_create(&threads[4], NULL, t5, NULL);
    pthread_create(&threads[5], NULL, t6, NULL);
    sleep(2);
    printf("Test 25: 6-thread hexagon DEADLOCK\n");
    return 1;
}
