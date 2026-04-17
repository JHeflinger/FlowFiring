#include "easymemory.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

size_t g_ezn_allocated_bytes = 0;

void* ez_reallocate(void* ptr, size_t amount, size_t size) {
    void* original_ptr = (void*)((uint8_t*)ptr - sizeof(size_t));
    size_t ptrsize = 0;
    memcpy(&ptrsize, original_ptr, sizeof(size_t));
    size_t newsize = (amount * size) + sizeof(size_t);
    void* new_ptr = realloc(original_ptr, newsize);
    if (newsize > ptrsize) memset(new_ptr + ptrsize, 0, newsize - ptrsize);
    memcpy(new_ptr, &newsize, sizeof(size_t));
    g_ezn_allocated_bytes -= ptrsize;
    g_ezn_allocated_bytes += newsize;
    return (void*)((uint8_t*)new_ptr + sizeof(size_t));
}

void* ez_allocate(size_t amount, size_t size) {
	g_ezn_allocated_bytes += (amount * size) + sizeof(size_t);
	size_t numbytes = (amount * size) + sizeof(size_t);
	void* full_ptr = calloc(numbytes, sizeof(uint8_t));
	memcpy(full_ptr, &numbytes, sizeof(size_t));
	return (void*)((uint8_t*)full_ptr + sizeof(size_t));
}

void ez_free(void* ptr) {
	if (ptr == NULL) return;
	size_t ptrsize = 0;
	void* full_ptr = (void*)((uint8_t*)ptr - sizeof(size_t));
	memcpy(&ptrsize, full_ptr, sizeof(size_t));
	g_ezn_allocated_bytes -= ptrsize;
	free(full_ptr);
}

size_t ez_allocated_bytes() {
	return g_ezn_allocated_bytes;
}
