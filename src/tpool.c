#include "tpool.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct worker_arg {
    tpool *pool;
    size_t index;
} worker_arg;

static void *worker(void *__args) {
    worker_arg args = *(worker_arg *) __args;
    tpool *pool = args.pool;
    task task;

    while (!pool->shutdown) {
        pool->status[args.index] = 1; // waiting for mutex

        pthread_mutex_lock(&pool->mutex_queue);
        pool->status[args.index] = 2; // waiting for task
        while (task_queue_count(&pool->queue) == 0 && !pool->shutdown)
            pthread_cond_wait(&pool->cond_queue, &pool->mutex_queue);
        
        if (!pool->shutdown)
            task_dequeue(&pool->queue, &task);

        pthread_mutex_unlock(&pool->mutex_queue);

        if (pool->shutdown)
            break;

        pool->status[args.index] = 3; // running task
        task.start_routine(task.args);
        // TODO: handle return value
    }

    pool->status[args.index] = 4; // shutdown
    
    free(__args);
    pthread_exit(0);
}

int tpool_init(tpool *pool, size_t thread_count) {
    if (pool == NULL || thread_count == 0)
        return -1;

    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    pool->status = malloc(sizeof(size_t) * thread_count);
    pool->thread_count = thread_count;
    pool->shutdown = 0;
    pool->__task_id_counter = 0;

    memset(pool->status, 0, thread_count * sizeof(size_t));

    pthread_mutex_init(&pool->mutex_queue, NULL);
    pthread_cond_init(&pool->cond_queue, NULL);
    task_queue_init(&pool->queue, 2 * thread_count);

    for (int i = 0; i < thread_count; i++) {
        worker_arg* args = malloc(sizeof(worker_arg));
        args->pool = pool;
        args->index = i;
        if (pthread_create(pool->threads + i, NULL, worker, args) != 0) {
            tpool_destroy(pool);
            return -1;
        }
    }

    return 0;
}

int tpool_join(tpool *pool) {
    if (pool == NULL || pool->thread_count == 0 || pool->threads == NULL)
        return -1;

    for (int i = 0; i < pool->thread_count; i++)
        if (pthread_join(pool->threads[i], NULL) != 0)
            return -1;

    return 0;
}

int tpool_destroy(tpool *pool) {
    if (pool == NULL) 
        return -1;

    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond_queue);
    tpool_join(pool);

    if (task_queue_count(&pool->queue) > 0)
        fprintf(stderr, "[WARN] there are pending tasks in queue");

    task_queue_destroy(&pool->queue);
    free(pool->threads);
    free(pool->status);

    return 0;
}

ssize_t tpool_add_task(tpool *pool, routine start_routine, void *args) {
    if (pool == NULL || start_routine == NULL)
        return -1;

    ssize_t ret = -1;

    pthread_mutex_lock(&pool->mutex_queue);
    
    task task = {
        .start_routine = start_routine,
        .args = args,
        .id = pool->__task_id_counter++
    };
    
    ret = task.id;
    if (task_enqueue(&pool->queue, task) < 0)
        ret = -1;
    
    pthread_mutex_unlock(&pool->mutex_queue);
    pthread_cond_signal(&pool->cond_queue);
    
    return ret;
}

// queue

static int resize_task_queue(task_queue *queue, size_t new_capacity) {
    if (queue->front == -1)
        return -1;

    new_capacity = new_capacity < queue->min_capacity ? queue->min_capacity : new_capacity;

    task* new_tasks = malloc(sizeof(task) * new_capacity);
    if (new_tasks == NULL)
        return -1;
    
    for (int i = 0; i < task_queue_count(queue); i++)
        new_tasks[i] = queue->tasks[(queue->front + i) % queue->capacity];

    free(queue->tasks);
    queue->tasks = new_tasks;
    queue->rear = task_queue_count(queue) - 1;
    queue->front = 0;
    queue->capacity = new_capacity;

    return 0;
}

int task_queue_init(task_queue *queue, size_t capacity) {
    if (queue == NULL || capacity == 0)
        return -1;

    queue->tasks = malloc(sizeof(task) * capacity);
    if (queue->tasks == NULL)
        return -2;

    queue->front = -1;
    queue->rear = -1;
    queue->capacity = capacity;
    queue->min_capacity = capacity;

    return 0;
}

int task_queue_destroy(task_queue *queue) {
    if (queue == NULL)
        return -1;

    free(queue->tasks);

    return 0;
}

int task_enqueue(task_queue *queue, task task) {
    if (queue == NULL)
        return -1;

    // queue full
    if ((queue->rear + 1) % queue->capacity == queue->front) {
        if (resize_task_queue(queue, 2 * queue->capacity) < 0)
            return -2;
    }
    
    queue->rear = (queue->rear + 1) % queue->capacity;
    if (queue->front == -1)
        queue->front = queue->rear;

    queue->tasks[queue->rear] = task;

    return 0;
}

int task_dequeue(task_queue *queue, task *task) {
    if (queue == NULL)
        return -1;

    // queue empty
    if (queue->front == -1)
        return -2;

    if (task != NULL)
        *task = queue->tasks[queue->front];

    queue->front = (queue->front + 1) % queue->capacity;
    
    // queue emptied
    if (queue->front == (queue->rear + 1) % queue->capacity)
        queue->front = queue->rear = -1;
    
    if (task_queue_count(queue) < queue->capacity / 4)
        resize_task_queue(queue, queue->capacity / 2);

    return 0;
}

ssize_t task_queue_count(task_queue *queue) {
    if (queue == NULL)
        return -1;

    if (queue->front == -1)
        return 0;

    if (queue->front > queue->rear)
        return queue->capacity - queue->front + queue->rear + 1;

    return queue->rear - queue->front + 1;
}