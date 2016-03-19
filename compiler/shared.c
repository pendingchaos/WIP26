#include "shared.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_ALLOCS 262144

typedef struct alloc_t {
    void* ptr;
    size_t amount;
    const char* file;
    int line;
    const char* func;
} alloc_t;

static size_t alloc_count = 0;
alloc_t allocs[MAX_ALLOCS];

void* _alloc_mem(size_t amount, const char* file, int line, const char* func) {
    void* ptr = calloc(1, amount);
    if (!ptr && amount) {
        fprintf(stderr, "Failed to allocate memory.\n");
        abort();
    }
    
    alloc_t* alloc = allocs + alloc_count++;
    alloc->ptr = ptr;
    alloc->amount = amount;
    alloc->file = file;
    alloc->line = line;
    alloc->func = func;
    
    return ptr;
}

void* _realloc_mem(void* ptr, size_t amount, const char* file, int line, const char* func) {
    ptr = realloc(ptr, amount);
    if (!ptr && amount) {
        fprintf(stderr, "Failed to allocate memory.\n");
        abort();
    }
    
    alloc_t* alloc = NULL;
    for (size_t i = 0; i < alloc_count; i++)
        if (allocs[i].ptr == ptr) {
            alloc = allocs + i;
            break;
        }
    
    if (alloc) {
        alloc->amount = amount;
        alloc->file = file;
        alloc->line = line;
        alloc->func = func;
    }
    
    return ptr;
}

void print_mem_stats() {
    size_t amount = 0;
    for (size_t i = 0; i < alloc_count; i++) amount += allocs[i].amount;
    
    printf("%zu allocations\n", alloc_count);
    printf("%zu bytes allocated\n", amount);
    
    size_t loc_count = 0;
    const char** files = NULL;
    int* lines = NULL;
    const char** funcs = NULL;
    size_t* amounts = NULL;
    
    for (size_t i = 0; i < alloc_count; i++) {
        alloc_t* alloc = allocs + i;
        for (size_t j = 0; j < loc_count; j++)
            if (!strcmp(files[j], alloc->file) &&
                !strcmp(funcs[j], alloc->func) &&
                lines[j] == alloc->line) {
                amounts[j] += alloc->amount;
                goto end;
            }
        
        files = realloc(files, (loc_count+1)*sizeof(const char*));
        lines = realloc(lines, (loc_count+1)*sizeof(int));
        funcs = realloc(funcs, (loc_count+1)*sizeof(const char*));
        amounts = realloc(amounts, (loc_count+1)*sizeof(size_t));
        
        files[loc_count] = alloc->file;
        lines[loc_count] = alloc->line;
        funcs[loc_count] = alloc->func;
        amounts[loc_count] = alloc->amount;
        loc_count++;
        
        end:;
    }
    
    for (size_t i = 0; i < loc_count; i++)
        printf("%s:%d:%s: %zu bytes (%.0f%c)\n", files[i], lines[i], funcs[i], amounts[i], (amounts[i]/(float)amount)*100.0, '%');
    
    free(files);
    free(lines);
    free(funcs);
    free(amounts);
}

void* _append_mem(void* ptr, size_t count, size_t item_size, void* data, const char* file, int line, const char* func) {
    ptr = _realloc_mem(ptr, (count+1)*item_size, file, line, func);
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

void* _mem_group_alloc(mem_group_t* group, size_t amount, const char* file, int line, const char* func) {
    if (!amount) amount = 1;
    void* alloc = _alloc_mem(amount, file, line, func);
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

char* _copy_str(const char* str, const char* file, int line, const char* func) {
    char* res = _alloc_mem(strlen(str)+1, file, line, func);
    strcpy(res, str);
    return res;
}

char* _copy_str_group(mem_group_t* group, const char* str, const char* file, int line, const char* func) {
    char* res = _mem_group_alloc(group, strlen(str)+1, file, line, func);
    strcpy(res, str);
    return res;
}
