#include "threading.h"

#include <string.h>

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
