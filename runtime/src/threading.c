#include "threading.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static bool null_destroy(threading_t* _) {
    return true;
}

static thread_res_t null_run(threading_t* _, thread_func_t func, size_t count, void* data) {
    void* ptr = func(0, count, data);
    
    thread_res_t res;
    res.success = true;
    res.count = 1;
    res.res[0] = ptr;
    return res;
}

bool create_null_threading(threading_t* threading) {
    memset(threading->error, 0, sizeof(threading->error));
    threading->destroy = &null_destroy;
    threading->run = &null_run;
    threading->internal = NULL;
    return true;
}

bool destroy_threading(threading_t* threading) {
    return threading->destroy(threading);
}

thread_res_t threading_run(threading_t* threading, thread_func_t func, size_t count, void* data) {
    return threading->run(threading, func, count, data);
}

#if 0
typedef struct pthread_internal_t {
    size_t count;
} pthread_internal_t;

typedef struct pthread_data_t {
    thread_func_t func;
    size_t begin;
    size_t count;
    void* userdata;
    void* res;
} pthread_data_t;

static bool pthread_destroy(threading_t* threading) {
    free(threading->internal);
    return true;
}

static void* pthread_func(void* userdata) {
    pthread_data_t* data = userdata;
    
    return data->func(data->begin, data->count, data->userdata);
}

static thread_res_t pthread_run(threading_t* threading, thread_func_t func, size_t count, void* data) {
    size_t thread_count = ((pthread_internal_t*)threading->internal)->count;
    if (count < thread_count) thread_count = count;
    pthread_t threads[thread_count];
    pthread_data_t thread_data[thread_count];
    
    size_t begin = 0;
    for (size_t i = 0; i < thread_count; i++) {
        thread_data[i].func = func;
        thread_data[i].begin = begin;
        thread_data[i].count = count/(thread_count+1);
        thread_data[i].userdata = data;
        pthread_create(threads+i, NULL, &pthread_func, thread_data+i);
        
        begin += thread_data[i].count;
    }
    
    thread_res_t res;
    res.success = true;
    res.count = thread_count+1;
    res.res[0] = func(begin, count-begin, data);
    
    for (size_t i = 0; i < thread_count; i++)
        pthread_join(threads[i], res.res+i+1);
    
    return res;
}

bool create_builtin_threading(threading_t* threading, size_t count) {
    if (!count) count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > MAX_THREADS) count = MAX_THREADS;
    
    memset(threading->error, 0, sizeof(threading->error));
    threading->destroy = &pthread_destroy;
    threading->run = &pthread_run;
    threading->internal = malloc(sizeof(pthread_internal_t));
    if (!threading->internal) ;//TODO
    
    pthread_internal_t* internal = threading->internal;
    internal->count = count;
    
    return true;
}
#else
#include <semaphore.h>

typedef struct pthread_data_t {
    thread_func_t func;
    size_t begin;
    size_t count;
    void* userdata;
    void* res;
    sem_t* begin_sem;
    sem_t* end_sem;
} pthread_data_t;

typedef struct pthread_internal_t {
    size_t count;
    pthread_t threads[MAX_THREADS];
    sem_t begin_sems[MAX_THREADS];
    sem_t end_sems[MAX_THREADS];
    pthread_data_t data[MAX_THREADS];
} pthread_internal_t;

static bool pthread_destroy(threading_t* threading) {
    pthread_internal_t* internal = threading->internal;
    for (size_t i = 0; i < internal->count; i++) {
        pthread_cancel(internal->threads[i]);
        sem_destroy(internal->end_sems+i);
        sem_destroy(internal->begin_sems+i);
    }
    free(internal);
    return true;
}

static void* pthread_func(void* userdata) {
    pthread_data_t* data = userdata;
    
    while (true) {
        sem_wait(data->begin_sem);
        void* res = data->func(data->begin, data->count, data->userdata);
        data->res = res;
        sem_post(data->end_sem);
    }
    
    return NULL;
}

static thread_res_t pthread_run(threading_t* threading, thread_func_t func, size_t count, void* data) {
    pthread_internal_t* internal = threading->internal;
    
    size_t begin = 0;
    for (size_t i = 0; i < internal->count; i++) {
        internal->data[i].func = func;
        internal->data[i].begin = begin;
        internal->data[i].count = count/(internal->count+1);
        internal->data[i].userdata = data;
        begin += internal->data[i].count;
    }
    
    for (size_t i = 0; i < internal->count; i++)
        sem_post(internal->begin_sems+i);
    
    thread_res_t res;
    res.success = true;
    res.count = internal->count+1;
    res.res[0] = func(begin, count-begin, data);
    
    for (size_t i = 0; i < internal->count; i++)
        sem_wait(internal->end_sems+i);
    
    for (size_t i = 0; i < internal->count; i++)
        res.res[i+1] = internal->data[i].res;
    
    return res;
}

bool create_builtin_threading(threading_t* threading, size_t count) {
    if (!count) count = sysconf(_SC_NPROCESSORS_ONLN)+1;
    if (count > MAX_THREADS) count = MAX_THREADS;
    
    memset(threading->error, 0, sizeof(threading->error));
    threading->destroy = &pthread_destroy;
    threading->run = &pthread_run;
    threading->internal = malloc(sizeof(pthread_internal_t));
    if (!threading->internal) ;//TODO
    
    pthread_internal_t* internal = threading->internal;
    internal->count = count;
    
    for (size_t i = 0; i < count; i++) {
        sem_init(internal->end_sems+i, 0, 0);
        sem_init(internal->begin_sems+i, 0, 0);
    }
    
    for (size_t i = 0; i < internal->count; i++) {
        internal->data[i].begin_sem = internal->begin_sems + i;
        internal->data[i].end_sem = internal->end_sems + i;
    }
    
    for (size_t i = 0; i < count; i++)
        pthread_create(internal->threads+i, NULL, &pthread_func, internal->data+i);
    
    return true;
}
#endif
