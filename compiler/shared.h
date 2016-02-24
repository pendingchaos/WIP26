#ifndef SHARED_H
#define SHARED_H
#include <stddef.h>

typedef struct {
    size_t count;
    void** allocs;
} mem_group_t;

void* alloc_mem(size_t amount);
void* realloc_mem(void* ptr, size_t amount);
void* append_mem(void* ptr, size_t count, size_t item_size, void* data);
char* read_file(const char* filename);
mem_group_t* create_mem_group();
void destroy_mem_group(mem_group_t* group);
void* mem_group_alloc(mem_group_t* group, size_t amount);
void mem_group_replace(mem_group_t* group, void* old, void* new);
char* copy_str(const char* str);
char* copy_str_group(mem_group_t* group, const char* str);
#endif
