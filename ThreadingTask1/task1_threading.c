
/*
 * ST5004CEM - Task 1: Process Management and Threading
 * task1_threading.c
 *
 * Demonstrates:
 *   1. Multiple concurrent threads (>= 3)
 *   2. Synchronization: mutex, semaphore, condition variable (monitor)
 *   3. Race condition (unsynchronized) vs fixed (synchronized) behaviour
 *   4. Deadlock (inconsistent lock order) vs prevention (consistent order)
 *
 * Compile:  gcc task1_threading.c -o task1_threading -lpthread
 * Run:      ./task1_threading
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define NUM_THREADS 5
#define INCREMENTS 200000

long shared_counter = 0;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
int use_lock_flag = 0;

void *increment_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < INCREMENTS; i++) {
        if (use_lock_flag) {
            pthread_mutex_lock(&counter_lock);
            long tmp = shared_counter;
            tmp++;
            shared_counter = tmp;
            pthread_mutex_unlock(&counter_lock);
        } else {
            long tmp = shared_counter;
            tmp++;
            shared_counter = tmp;
        }
    }
    return NULL;
}

void run_counter_demo(int use_lock) {
    shared_counter = 0;
    use_lock_flag = use_lock;
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, increment_worker, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    long expected = (long)NUM_THREADS * INCREMENTS;
    printf("[%s] expected=%ld, actual=%ld, lost_updates=%ld\n",
           use_lock ? "WITH lock (safe)" : "WITHOUT lock (race condition)",
           expected, shared_counter, expected - shared_counter);
}

sem_t connection_pool;

void *semaphore_worker(void *arg) {
    long id = (long)arg;
    printf("[Worker-%ld] waiting for a connection...\n", id);
    sem_wait(&connection_pool);
    printf("[Worker-%ld] acquired connection\n", id);
    usleep(300000 + (rand() % 400000));
    printf("[Worker-%ld] releasing connection\n", id);
    sem_post(&connection_pool);
    return NULL;
}

void run_semaphore_demo(void) {
    sem_init(&connection_pool, 0, 2);
    pthread_t threads[5];
    for (long i = 0; i < 5; i++)
        pthread_create(&threads[i], NULL, semaphore_worker, (void *)i);
    for (int i = 0; i < 5; i++)
        pthread_join(threads[i], NULL);
    sem_destroy(&connection_pool);
}

#define BUFFER_CAPACITY 3
#define ITEMS_TO_PRODUCE 10

int buffer[BUFFER_CAPACITY];
int buf_count = 0;
pthread_mutex_t buf_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

void *producer(void *arg) {
    (void)arg;
    for (int i = 0; i < ITEMS_TO_PRODUCE; i++) {
        pthread_mutex_lock(&buf_lock);
        while (buf_count >= BUFFER_CAPACITY)
            pthread_cond_wait(&not_full, &buf_lock);
        buffer[buf_count++] = i;
        printf("[Producer] produced %d (buffer count=%d)\n", i, buf_count);
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&buf_lock);
        usleep(50000 + (rand() % 150000));
    }
    return NULL;
}

void *consumer(void *arg) {
    (void)arg;
    for (int i = 0; i < ITEMS_TO_PRODUCE; i++) {
        pthread_mutex_lock(&buf_lock);
        while (buf_count == 0)
            pthread_cond_wait(&not_empty, &buf_lock);
        int item = buffer[--buf_count];
        printf("[Consumer] consumed %d (buffer count=%d)\n", item, buf_count);
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&buf_lock);
        usleep(100000 + (rand() % 200000));
    }
    return NULL;
}

void run_producer_consumer_demo(void) {
    buf_count = 0;
    pthread_t p, c;
    pthread_create(&p, NULL, producer, NULL);
    pthread_create(&c, NULL, consumer, NULL);
    pthread_join(p, NULL);
    pthread_join(c, NULL);
}

pthread_mutex_t resource_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t resource_2 = PTHREAD_MUTEX_INITIALIZER;

int lock_with_timeout(pthread_mutex_t *m, int seconds) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += seconds;
    return pthread_mutex_timedlock(m, &ts);
}

void *deadlock_worker_a(void *arg) {
    (void)arg;
    pthread_mutex_lock(&resource_1);
    printf("[A] locked resource_1, trying resource_2...\n");
    usleep(200000);
    if (lock_with_timeout(&resource_2, 2) == 0) {
        printf("[A] locked resource_2\n");
        pthread_mutex_unlock(&resource_2);
    } else {
        printf("[A] TIMED OUT waiting for resource_2 -> deadlock detected\n");
    }
    pthread_mutex_unlock(&resource_1);
    return NULL;
}

void *deadlock_worker_b(void *arg) {
    (void)arg;
    pthread_mutex_lock(&resource_2);
    printf("[B] locked resource_2, trying resource_1...\n");
    usleep(200000);
    if (lock_with_timeout(&resource_1, 2) == 0) {
        printf("[B] locked resource_1\n");
        pthread_mutex_unlock(&resource_1);
    } else {
        printf("[B] TIMED OUT waiting for resource_1 -> deadlock detected\n");
    }
    pthread_mutex_unlock(&resource_2);
    return NULL;
}

void run_deadlock_demo(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, deadlock_worker_a, NULL);
    pthread_create(&b, NULL, deadlock_worker_b, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
}

void *safe_worker_a(void *arg) {
    (void)arg;
    pthread_mutex_lock(&resource_1);
    printf("[A-safe] locked resource_1\n");
    usleep(200000);
    pthread_mutex_lock(&resource_2);
    printf("[A-safe] locked resource_2\n");
    pthread_mutex_unlock(&resource_2);
    pthread_mutex_unlock(&resource_1);
    return NULL;
}

void *safe_worker_b(void *arg) {
    (void)arg;
    pthread_mutex_lock(&resource_1);
    printf("[B-safe] locked resource_1\n");
    usleep(200000);
    pthread_mutex_lock(&resource_2);
    printf("[B-safe] locked resource_2\n");
    pthread_mutex_unlock(&resource_2);
    pthread_mutex_unlock(&resource_1);
    return NULL;
}

void run_deadlock_prevention_demo(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, safe_worker_a, NULL);
    pthread_create(&b, NULL, safe_worker_b, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
}

int main(void) {
    srand((unsigned int)time(NULL));

    printf("======================================================================\n");
    printf("DEMO 1: Race condition (no synchronization)\n");
    printf("======================================================================\n");
    run_counter_demo(0);

    printf("\n======================================================================\n");
    printf("DEMO 2: Fixed with mutex\n");
    printf("======================================================================\n");
    run_counter_demo(1);

    printf("\n======================================================================\n");
    printf("DEMO 3: Semaphore - limited resource pool (2 slots, 5 workers)\n");
    printf("======================================================================\n");
    run_semaphore_demo();

    printf("\n======================================================================\n");
    printf("DEMO 4: Monitor - Producer/Consumer with condition variables\n");
    printf("======================================================================\n");
    run_producer_consumer_demo();

    printf("\n======================================================================\n");
    printf("DEMO 5: Deadlock (inconsistent lock ordering)\n");
    printf("======================================================================\n");
    run_deadlock_demo();

    printf("\n======================================================================\n");
    printf("DEMO 6: Deadlock prevention (consistent lock ordering)\n");
    printf("======================================================================\n");
    run_deadlock_prevention_demo();

    return 0;
}
