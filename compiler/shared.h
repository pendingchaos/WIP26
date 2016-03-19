#ifndef SHARED_H
#define SHARED_H
#include <stddef.h>

typedef struct {
    size_t count;
    void** allocs;
} mem_group_t;

#define alloc_mem(amount) _alloc_mem(amount, __FILE__, __LINE__, __FUNCTION__)
#define realloc_mem(ptr, amount) _realloc_mem(ptr, amount, __FILE__, __LINE__, __FUNCTION__)
#define append_mem(ptr, count, item_size, data) _append_mem(ptr, count, item_size, data, __FILE__, __LINE__, __FUNCTION__)
#define mem_group_alloc(group, amount) _mem_group_alloc(group, amount, __FILE__, __LINE__, __FUNCTION__)
#define copy_str(str) _copy_str(str, __FILE__, __LINE__, __FUNCTION__)
#define copy_str_group(group, str) _copy_str_group(group, str, __FILE__, __LINE__, __FUNCTION__)

char* read_file(const char* filename);
void print_mem_stats();
mem_group_t* create_mem_group();
void destroy_mem_group(mem_group_t* group);
void mem_group_replace(mem_group_t* group, void* old, void* new);

void* _alloc_mem(size_t amount, const char* file, int line, const char* func);
void* _realloc_mem(void* ptr, size_t amount, const char* file, int line, const char* func);
void* _append_mem(void* ptr, size_t count, size_t item_size, void* data, const char* file, int line, const char* func);
void* _mem_group_alloc(mem_group_t* group, size_t amount, const char* file, int line, const char* func);
char* _copy_str(const char* str, const char* file, int line, const char* func);
char* _copy_str_group(mem_group_t* group, const char* str, const char* file, int line, const char* func);
#endif
