#ifndef __H_TPOOL__
#define __H_TPOOL__

#include <pthread.h>
#include <sys/types.h>

typedef void* (*routine)(void*);

typedef struct task {
    routine start_routine;
    void *args;
    size_t id;
} task;

typedef struct task_queue {
    task *tasks;
    size_t front;
    size_t rear;
    size_t capacity;
    size_t min_capacity;
} task_queue;

typedef struct tpool {
    pthread_t       *threads;
    size_t          *status;
    pthread_mutex_t mutex_queue;
    pthread_cond_t  cond_queue;
    task_queue      queue;
    size_t          thread_count;
    size_t          shutdown:1;
    size_t          __task_id_counter;
} tpool;

int tpool_init(tpool *pool, size_t thread_count);
int tpool_join(tpool *pool);
int tpool_destroy(tpool *pool);
ssize_t tpool_add_task(tpool *pool, routine start_routine, void *args);

int task_queue_init(task_queue *queue, size_t capacity);
int task_queue_destroy(task_queue *queue);
int task_enqueue(task_queue *queue, task task);
int task_dequeue(task_queue *queue, task *task);
ssize_t task_queue_count(task_queue *queue);

#endif