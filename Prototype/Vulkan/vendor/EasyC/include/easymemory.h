#ifndef EASYMEMORY_H
#define EASYMEMORY_H

#include <stddef.h>

void* ez_reallocate(void* ptr, size_t amount, size_t size);
void* ez_allocate(size_t amount, size_t size);
void ez_free(void* ptr);
size_t ez_allocated_bytes();

#define EZ_REALLOC(pointer, numthings, thingsize) ez_reallocate(pointer, numthings, thingsize)
#define EZ_ALLOC(numthings, thingsize) ez_allocate(numthings, thingsize)
#define EZ_FREE(pointer) ez_free((void*)pointer)
#define EZ_ALLOCATED() ez_allocated_bytes()

#endif
