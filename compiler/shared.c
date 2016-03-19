#include "shared.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

void* alloc_mem(size_t amount) {
    void* ptr = calloc(1, amount);
    if (!ptr && amount) {
        fprintf(stderr, "Failed to allocate memory.\n");
        abort();
    }
    return ptr;
}

void* realloc_mem(void* ptr, size_t amount) {
    ptr = realloc(ptr, amount);
    if (!ptr && amount) {
        fprintf(stderr, "Failed to allocate memory.\n");
        abort();
    }
    return ptr;
}

void* append_mem(void* ptr, size_t count, size_t item_size, void* data) {
    ptr = realloc_mem(ptr, (count+1)*item_size);
    memcpy(((uint8_t*)ptr)+count*item_size, data, item_size);
    return ptr;
}

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    
    size_t length = 0;
    char* source = NULL;
    size_t read;
    char data[16384];
    while ((read = fread(data, 1, sizeof(data), file))) {
        source = realloc_mem(source, length+read);
        memcpy(source+length, data, read);
        length += read;
    }
    source = realloc_mem(source, length+1);
    source[length] = 0;
    
    fclose(file);
    
    return source;
}

mem_group_t* create_mem_group() {
    mem_group_t* group = alloc_mem(sizeof(mem_group_t));
    group->count = 0;
    group->allocs = NULL;
    return group;
}

void destroy_mem_group(mem_group_t* group) {
    for (size_t i = 0; i < group->count; i++) free(group->allocs[i]);
    free(group->allocs);
    free(group);
}

void* mem_group_alloc(mem_group_t* group, size_t amount) {
    if (!amount) amount = 1;
    void* alloc = alloc_mem(amount);
    group->allocs = append_mem(group->allocs, group->count++, sizeof(void*), &alloc);
    return alloc;
}

void mem_group_replace(mem_group_t* group, void* old, void* new) {
    for (size_t i = 0; i < group->count; i++)
        if (group->allocs[i] == old) {
            free(group->allocs[i]);
            group->allocs[i] = new;
            return;
        }
}

char* copy_str(const char* str) {
    char* res = alloc_mem(strlen(str)+1);
    strcpy(res, str);
    return res;
}

char* copy_str_group(mem_group_t* group, const char* str) {
    char* res = mem_group_alloc(group, strlen(str)+1);
    strcpy(res, str);
    return res;
}
