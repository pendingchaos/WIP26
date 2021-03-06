#ifndef THREADING_H
#define THREADING_H
#include <stdbool.h>
#include <stddef.h>

#define MAX_THREADS 256

typedef struct threading_t threading_t;
typedef struct thread_run_t thread_run_t;
typedef struct thread_res_t thread_res_t;
typedef void* (*thread_func_t)(size_t, size_t, void*);

struct threading_t {
    char error[256];
    bool (*destroy)(threading_t*);
    thread_res_t (*run)(threading_t*, thread_run_t);
    void* (*create_mutex)(threading_t*);
    void (*destroy_mutex)(threading_t*, void*);
    void (*lock_mutex)(threading_t*, void*);
    void (*unlock_mutex)(threading_t*, void*);
    void* internal;
};

struct thread_run_t {
    thread_func_t func;
    size_t count;
    void* data;
};

struct thread_res_t {
    bool success;
    size_t count;
    void* res[MAX_THREADS];
};

bool create_builtin_threading(threading_t* threading, size_t count);
bool create_null_threading(threading_t* threading);
bool destroy_threading(threading_t* threading);
thread_res_t threading_run(threading_t* threading, thread_run_t run);

void* create_mutex(threading_t* threading);
void destroy_mutex(threading_t* threading, void* mut);
void lock_mutex(threading_t* threading, void* mut);
void unlock_mutex(threading_t* threading, void* mut);
#endif
