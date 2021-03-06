#include "threading.h"

#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

bool destroy_threading(threading_t* threading) {
    return threading->destroy(threading);
}

thread_res_t threading_run(threading_t* threading, thread_run_t run) {
    return threading->run(threading, run);
}

void* create_mutex(threading_t* threading) {
    return threading->create_mutex(threading);
}

void destroy_mutex(threading_t* threading, void* mut) {
    threading->destroy_mutex(threading, mut);
}

void lock_mutex(threading_t* threading, void* mut) {
    threading->lock_mutex(threading, mut);
}

void unlock_mutex(threading_t* threading, void* mut) {
    threading->unlock_mutex(threading, mut);
}

static bool set_error(threading_t* threading, const char* message) {
    strncpy(threading->error, message, sizeof(threading->error));
    return false;
}

static bool null_destroy(threading_t* _) {
    return true;
}

static thread_res_t null_run(threading_t* _, thread_run_t run) {
    void* ptr = run.func(0, run.count, run.data);
    
    thread_res_t res;
    res.success = true;
    res.count = 1;
    res.res[0] = ptr;
    return res;
}

static void* null_create_mutex(threading_t* threading) {
    return threading;
}

static void null_destroy_mutex(threading_t* threading, void* mut) {
}

static void null_lock_mutex(threading_t* threading, void* mut) {
}

static void null_unlock_mutex(threading_t* threading, void* mut) {
}

bool create_null_threading(threading_t* threading) {
    memset(threading->error, 0, sizeof(threading->error));
    threading->destroy = &null_destroy;
    threading->run = &null_run;
    threading->create_mutex = &null_create_mutex;
    threading->destroy_mutex = &null_destroy_mutex;
    threading->lock_mutex = &null_lock_mutex;
    threading->unlock_mutex = &null_unlock_mutex;
    threading->internal = NULL;
    return true;
}

typedef struct pthread_data_t {
    thread_func_t func;
    size_t begin;
    size_t count;
    void* userdata;
    void* res;
    sem_t* begin_sem;
    sem_t* end_sem;
    bool destroy;
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
        internal->data[i].destroy = true;
        sem_post(internal->begin_sems+i);
        void* res;
        pthread_join(internal->threads[i], &res);
    }
    
    for (size_t i = 0; i < internal->count; i++) {
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
        
        if (data->destroy) return NULL;
        
        void* res = data->func(data->begin, data->count, data->userdata);
        data->res = res;
        
        sem_post(data->end_sem);
    }
}

static thread_res_t pthread_run(threading_t* threading, thread_run_t run) {
    pthread_internal_t* internal = threading->internal;
    
    size_t begin = 0;
    for (size_t i = 0; i < internal->count; i++) {
        internal->data[i].func = run.func;
        internal->data[i].begin = begin;
        internal->data[i].count = run.count/(internal->count+1);
        internal->data[i].userdata = run.data;
        begin += internal->data[i].count;
    }
    
    for (size_t i = 0; i < internal->count; i++)
        sem_post(internal->begin_sems+i);
    
    thread_res_t res;
    res.success = true;
    res.count = internal->count+1;
    res.res[0] = run.func(begin, run.count-begin, run.data);
    
    for (size_t i = 0; i < internal->count; i++)
        sem_wait(internal->end_sems+i);
    
    for (size_t i = 0; i < internal->count; i++)
        res.res[i+1] = internal->data[i].res;
    
    return res;
}

static void* pthread_create_mutex(threading_t* threading) {
    pthread_mutex_t* mut = malloc(sizeof(pthread_mutex_t));
    
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    pthread_mutex_init(mut, &attr);
    
    pthread_mutexattr_destroy(&attr);
    
    return mut;
}

static void pthread_destroy_mutex(threading_t* threading, void* mut) {
    pthread_mutex_destroy(mut);
    free(mut);
}

static void pthread_lock_mutex(threading_t* threading, void* mut) {
    pthread_mutex_lock(mut);
}

static void pthread_unlock_mutex(threading_t* threading, void* mut) {
    pthread_mutex_unlock(mut);
}

bool create_builtin_threading(threading_t* threading, size_t count) {
    if (!count) count = sysconf(_SC_NPROCESSORS_ONLN)*2;
    if (count > MAX_THREADS) count = MAX_THREADS;
    
    memset(threading->error, 0, sizeof(threading->error));
    threading->destroy = &pthread_destroy;
    threading->run = &pthread_run;
    threading->create_mutex = &pthread_create_mutex;
    threading->destroy_mutex = &pthread_destroy_mutex;
    threading->lock_mutex = &pthread_lock_mutex;
    threading->unlock_mutex = &pthread_unlock_mutex;
    threading->internal = malloc(sizeof(pthread_internal_t));
    if (!threading->internal) 
        return set_error(threading, "Failed to allocate threading internal data");
    
    pthread_internal_t* internal = threading->internal;
    internal->count = count;
    
    for (size_t i = 0; i < count; i++) {
        if (sem_init(internal->end_sems+i, 0, 0) < 0) {
            free(internal);
            threading->internal = NULL;
            return set_error(threading, "Failed to create semaphore");
        }
        if (sem_init(internal->begin_sems+i, 0, 0) < 0) {
            free(internal);
            threading->internal = NULL;
            return set_error(threading, "Failed to create semaphore");
        }
    }
    
    for (size_t i = 0; i < internal->count; i++) {
        internal->data[i].begin_sem = internal->begin_sems + i;
        internal->data[i].end_sem = internal->end_sems + i;
        internal->data[i].destroy = false;
    }
    
    for (size_t i = 0; i < count; i++)
        if (pthread_create(internal->threads+i, NULL, &pthread_func, internal->data+i)) {
            free(internal);
            threading->internal = NULL;
            return set_error(threading, "Failed to create thread");
        }
    
    return true;
}
