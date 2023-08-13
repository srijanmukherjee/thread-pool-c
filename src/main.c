#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/signal.h>

#include "tpool.h"

void *long_running_task(void *args);

tpool thread_pool;

void interrupt(int _) {
    printf("\e[?25h");
    tpool_destroy(&thread_pool);
    exit(1);
}

void add_task(int _) {
    tpool_add_task(&thread_pool, long_running_task, NULL);
    printf("%-128s\n", "added a task");
}

int main(void) {
    size_t thread_count = 64;
    // cpu_set_t cpuset;

    signal(SIGINT, interrupt);
    signal(SIGUSR1, add_task);

    // if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == 0)
    //     thread_count = CPU_COUNT(&cpuset);

    printf("thread count: %ld\n", thread_count);

    if (tpool_init(&thread_pool, thread_count) < 0) {
        fprintf(stderr, "tpool_init() failed\n");
        exit(1);
    }

    printf("[info] adding 100 tasks\n");
    int added_tasks = 0;
    for (int i = 0; i < 100; i++)
        if (tpool_add_task(&thread_pool, long_running_task, NULL) > -1)
            added_tasks++;

    printf("added %d tasks successfully\n", added_tasks);

    const char *states[5] = { "i", "m", "t", "r", "s"};

    printf("\e[?25l");
    
    while (1) {
        // printf worker status
        for (int i = 0; i < thread_pool.thread_count; i++)
            printf("%s ", states[thread_pool.status[i]]);

        printf("\r");
        fflush(stdout);
        usleep(1000000);
    }

    printf("\e[?25h");
    return 0;
}

void *long_running_task(void *args) {
    sleep(rand() % 10);
    // printf("(%d) task done\n", (pid_t) syscall(SYS_gettid));
    return 0;
}