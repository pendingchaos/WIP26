#ifndef THREADING_H
#define THREADING_H
#include <stdbool.h>
#include <stddef.h>

#define MAX_THREADS 256

typedef struct threading_t threading_t;
typedef struct builtin_threading_conf_t builtin_threading_conf_t;
typedef struct thread_res_t thread_res_t;
typedef void* (*thread_func_t)(size_t, size_t, void*);

struct threading_t {
    char error[256];
    bool (*destroy)(threading_t*);
    thread_res_t (*run)(threading_t*, thread_func_t, size_t, void*);
    void* internal;
};

struct thread_res_t {
    bool success;
    size_t count;
    void* res[MAX_THREADS];
};

bool create_builtin_threading(threading_t* threading, size_t count);
bool create_null_threading(threading_t* threading);
bool destroy_threading(threading_t* threading);
thread_res_t threading_run(threading_t* threading, thread_func_t func, size_t count, void* data);
#endif
