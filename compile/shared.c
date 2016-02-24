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
