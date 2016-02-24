#ifndef SHARED_H
#define SHARED_H
#include <stddef.h>

void* alloc_mem(size_t amount);
void* realloc_mem(void* ptr, size_t amount);
void* append_mem(void* ptr, size_t count, size_t item_size, void* data);
char* read_file(const char* filename);
#endif
