# Threadpool in C
A very basic thread pool implementation in C. 

## Instructions
```bash
./build.sh
./bin/tpool-example
```

Send SIGUSR1 signal to add a task to the pool.

**Note:** This is a very basic implementation, more features will be added in the near future.

## Usage

Include the `src/tpool.h` file into your project, and add `src/tpool.c` during compilation.

1. Creating a thread pool
```c
    int tpool_init(tpool *pool, size_t thread_count);
```

2. Adding a task to the pool
```c
ssize_t tpool_add_task(tpool *pool, void* (*start_routine)(void*), void* args);
```
**returns:** -1 on error, otherwise gives the assigned `task id`

3. Destroying thread pool
```c
int tpool_destroy(tpool *pool);
```
