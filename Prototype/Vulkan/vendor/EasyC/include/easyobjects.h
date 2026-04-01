#ifndef EASYOBJECTS_H
#define EASYOBJECTS_H

#include "easylogger.h"
#include "easymemory.h"
#include "easybool.h"
#include <stdint.h>
#include <string.h>

// dynamic structs
#define DECLARE_PAIR(T) \
typedef struct { \
	T value[2]; \
} PAIR_##T;

#define DECLARE_TRIPLET(T) \
typedef struct { \
	T value[3]; \
} TRIPLET_##T;

#define DECLARE_QUAD(T) \
typedef struct { \
	T value[4]; \
} QUAD_##T;

#define DECLARE_ARRLIST(T) \
typedef struct { \
	size_t size; \
	size_t maxsize; \
	T* data; \
} ARRLIST_##T; \
\
void ARRLIST_##T##_zero(ARRLIST_##T* list, size_t size); \
void ARRLIST_##T##_add(ARRLIST_##T* list, T element); \
void ARRLIST_##T##_insert(ARRLIST_##T* list, T element, size_t index); \
int ARRLIST_##T##_has(ARRLIST_##T* list, T element); \
void ARRLIST_##T##_remove(ARRLIST_##T* list, size_t index); \
T ARRLIST_##T##_get(ARRLIST_##T* list, size_t index); \
void ARRLIST_##T##_clear(ARRLIST_##T* list); \
void ARRLIST_##T##_wipe(ARRLIST_##T* list);

#define IMPL_ARRLIST(T) \
void ARRLIST_##T##_zero(ARRLIST_##T* list, size_t size) { \
    ARRLIST_##T##_clear(list); \
    list->maxsize = size; \
    list->size = size; \
    list->data = (T*)EZ_ALLOC(size, sizeof(T)); \
} \
\
void ARRLIST_##T##_add(ARRLIST_##T* list, T element) { \
	if (list->maxsize == 0) { \
		list->data = (T*)EZ_ALLOC(1, sizeof(T)); \
		list->size = 1; \
		list->maxsize = 1; \
		memcpy(list->data, &element, sizeof(T)); \
	} else if (list->size < list->maxsize) { \
		memcpy(&(list->data[list->size]), &element, sizeof(T)); \
		list->size++; \
	} else { \
		list->maxsize *= 2; \
        list->data = EZ_REALLOC(list->data, list->maxsize, sizeof(T)); \
		memcpy(&(list->data[list->size]), &element, sizeof(T)); \
		list->size++; \
	} \
} \
\
void ARRLIST_##T##_insert(ARRLIST_##T* list, T element, size_t index) { \
	if (index > list->size) { \
		EZ_FATAL("Invalid arraylist index to insert"); \
	} else if (index == list->size) { \
		ARRLIST_##T##_add(list, element); \
	} else { \
		ARRLIST_##T##_add(list, element); \
		for (size_t i = list->size; i > 0; i--) { \
			size_t ind = i - 1; \
			if (ind == index) { \
				memcpy(&(list->data[ind]), &element, sizeof(T)); \
				break; \
			} else { \
				memcpy(&(list->data[ind]), &(list->data[ind - 1]), sizeof(T)); \
			} \
		} \
	} \
} \
\
int ARRLIST_##T##_has(ARRLIST_##T* list, T element) { \
	for (size_t i = 0; i < list->size; i++) \
		if (memcmp(&element, &(list->data[i]), sizeof(T)) == 0) return TRUE; \
	return FALSE; \
} \
\
void ARRLIST_##T##_remove(ARRLIST_##T* list, size_t index) { \
	if (index >= list->size) \
		EZ_FATAL("Invalid arraylist index to remove"); \
	if (index == list->size - 1)  { \
		list->size--; \
		return; \
	} \
	for (size_t i = index; i < list->size - 1; i++) \
		memcpy(&(list->data[i]), &(list->data[i + 1]), sizeof(T)); \
	list->size--; \
} \
\
T ARRLIST_##T##_get(ARRLIST_##T* list, size_t index) { \
	if (index >= list->size) \
		EZ_FATAL("Invalid arraylist index to get"); \
	return list->data[index]; \
} \
\
void ARRLIST_##T##_clear(ARRLIST_##T* list) { \
	if (list->data != NULL) \
		EZ_FREE(list->data); \
	list->data = NULL; \
	list->size = 0; \
	list->maxsize = 0; \
} \
\
void ARRLIST_##T##_wipe(ARRLIST_##T* list) { \
	list->size = 0; \
}

#define DECLARE_ARR_ARRLIST(T) \
typedef struct { \
	size_t size; \
	size_t maxsize; \
	T* data; \
} ARRLIST_##T; \
\
void ARRLIST_##T##_zero(ARRLIST_##T* list, size_t size); \
void ARRLIST_##T##_add(ARRLIST_##T* list, T element); \
void ARRLIST_##T##_insert(ARRLIST_##T* list, T element, size_t index); \
int ARRLIST_##T##_has(ARRLIST_##T* list, T element); \
void ARRLIST_##T##_remove(ARRLIST_##T* list, size_t index); \
void ARRLIST_##T##_clear(ARRLIST_##T* list); \
void ARRLIST_##T##_wipe(ARRLIST_##T* list);

#define IMPL_ARR_ARRLIST(T) \
void ARRLIST_##T##_zero(ARRLIST_##T* list, size_t size) { \
    ARRLIST_##T##_clear(list); \
    list->maxsize = size; \
    list->size = size; \
    list->data = (T*)EZ_ALLOC(size, sizeof(T)); \
} \
\
void ARRLIST_##T##_add(ARRLIST_##T* list, T element) { \
	if (list->maxsize == 0) { \
		list->data = (T*)EZ_ALLOC(1, sizeof(T)); \
		list->size = 1; \
		list->maxsize = 1; \
		memcpy(list->data, element, sizeof(T)); \
	} else if (list->size < list->maxsize) { \
		memcpy(list->data[list->size], element, sizeof(T)); \
		list->size++; \
	} else { \
		list->maxsize *= 2; \
        list->data = EZ_REALLOC(list->data, list->maxsize, sizeof(T)); \
		memcpy(list->data[list->size], element, sizeof(T)); \
		list->size++; \
	} \
} \
\
void ARRLIST_##T##_insert(ARRLIST_##T* list, T element, size_t index) { \
	if (index > list->size) { \
		EZ_FATAL("Invalid arraylist index to insert"); \
	} else if (index == list->size) { \
		ARRLIST_##T##_add(list, element); \
	} else { \
		ARRLIST_##T##_add(list, element); \
		for (size_t i = list->size; i > 0; i--) { \
			size_t ind = i - 1; \
			if (ind == index) { \
				memcpy(list->data[ind], element, sizeof(T)); \
				break; \
			} else { \
				memcpy(list->data[ind], list->data[ind - 1], sizeof(T)); \
			} \
		} \
	} \
} \
\
int ARRLIST_##T##_has(ARRLIST_##T* list, T element) { \
	for (size_t i = 0; i < list->size; i++) \
		if (memcmp(element, list->data[i], sizeof(T)) == 0) return TRUE; \
	return FALSE; \
} \
\
void ARRLIST_##T##_remove(ARRLIST_##T* list, size_t index) { \
	if (index >= list->size) \
		EZ_FATAL("Invalid arraylist index to remove"); \
	if (index == list->size - 1)  { \
		list->size--; \
		return; \
	} \
	for (size_t i = index; i < list->size - 1; i++) \
		memcpy(list->data[i], list->data[i + 1], sizeof(T)); \
	list->size--; \
} \
\
void ARRLIST_##T##_clear(ARRLIST_##T* list) { \
	if (list->data != NULL) \
		EZ_FREE(list->data); \
	list->data = NULL; \
	list->size = 0; \
	list->maxsize = 0; \
} \
\
void ARRLIST_##T##_wipe(ARRLIST_##T* list) { \
	list->size = 0; \
}

#define DECLARE_ARRLIST_NAMED(name, T) \
typedef struct { \
	size_t size; \
	size_t maxsize; \
	T* data; \
} ARRLIST_##name; \
\
void ARRLIST_##name##_zero(ARRLIST_##name* list, size_t size); \
void ARRLIST_##name##_add(ARRLIST_##name* list, T element); \
void ARRLIST_##name##_insert(ARRLIST_##name* list, T element, size_t index); \
int ARRLIST_##name##_has(ARRLIST_##name* list, T element); \
void ARRLIST_##name##_remove(ARRLIST_##name* list, size_t index); \
T ARRLIST_##name##_get(ARRLIST_##name* list, size_t index); \
void ARRLIST_##name##_clear(ARRLIST_##name* list); \
void ARRLIST_##name##_wipe(ARRLIST_##name* list);

#define IMPL_ARRLIST_NAMED(name, T) \
void ARRLIST_##name##_zero(ARRLIST_##name* list, size_t size) { \
    ARRLIST_##name##_clear(list); \
    list->maxsize = size; \
    list->size = size; \
    list->data = (T*)EZ_ALLOC(size, sizeof(T)); \
} \
\
void ARRLIST_##name##_add(ARRLIST_##name* list, T element) { \
	if (list->maxsize == 0) { \
		list->data = (T*)EZ_ALLOC(1, sizeof(T)); \
		list->size = 1; \
		list->maxsize = 1; \
		memcpy(list->data, &element, sizeof(T)); \
	} else if (list->size < list->maxsize) { \
		memcpy(&(list->data[list->size]), &element, sizeof(T)); \
		list->size++; \
	} else { \
		list->maxsize *= 2; \
        list->data = EZ_REALLOC(list->data, list->maxsize, sizeof(T)); \
		memcpy(&(list->data[list->size]), &element, sizeof(T)); \
		list->size++; \
	} \
} \
\
void ARRLIST_##name##_insert(ARRLIST_##name* list, T element, size_t index) { \
	if (index > list->size) { \
		EZ_FATAL("Invalid arraylist index to insert"); \
	} else if (index == list->size) { \
		ARRLIST_##name##_add(list, element); \
	} else { \
		ARRLIST_##name##_add(list, element); \
		for (size_t i = list->size; i > 0; i--) { \
			size_t ind = i - 1; \
			if (ind == index) { \
				memcpy(&(list->data[ind]), &element, sizeof(T)); \
				break; \
			} else { \
				memcpy(&(list->data[ind]), &(list->data[ind - 1]), sizeof(T)); \
			} \
		} \
	} \
} \
\
int ARRLIST_##name##_has(ARRLIST_##name* list, T element) { \
	for (size_t i = 0; i < list->size; i++) \
		if (memcmp(&element, &(list->data[i]), sizeof(T)) == 0) return TRUE; \
	return FALSE; \
} \
\
void ARRLIST_##name##_remove(ARRLIST_##name* list, size_t index) { \
	if (index >= list->size) \
		EZ_FATAL("Invalid arraylist index to remove"); \
	if (index == list->size - 1)  { \
		list->size--; \
		return; \
	} \
	for (size_t i = index; i < list->size - 1; i++) \
		memcpy(&(list->data[i]), &(list->data[i + 1]), sizeof(T)); \
	list->size--; \
} \
\
T ARRLIST_##name##_get(ARRLIST_##name* list, size_t index) { \
	if (index >= list->size) \
		EZ_FATAL("Invalid arraylist index to get"); \
	return list->data[index]; \
} \
\
void ARRLIST_##name##_clear(ARRLIST_##name* list) { \
	if (list->data != NULL) \
		EZ_FREE(list->data); \
	list->data = NULL; \
	list->size = 0; \
	list->maxsize = 0; \
} \
\
void ARRLIST_##name##_wipe(ARRLIST_##name* list) { \
	list->size = 0; \
}

#define DECLARE_HASHMAP(Tkey, Tval, name) \
typedef struct { \
    Tkey key; \
    Tval value; \
    int used; \
} HASHENTRY_##name; \
\
typedef struct { \
    HASHENTRY_##name* entries; \
    size_t capacity; \
    size_t size; \
    size_t tombstones; \
} HASHMAP_##name; \
\
void HASHMAP_##name##_set(HASHMAP_##name* map, Tkey key, Tval value); \
BOOL HASHMAP_##name##_has(HASHMAP_##name* map, Tkey key); \
Tval HASHMAP_##name##_get(HASHMAP_##name* map, Tkey key); \
void HASHMAP_##name##_remove(HASHMAP_##name* map, Tkey key); \
void HASHMAP_##name##_clear(HASHMAP_##name* map);

#define IMPL_HASHMAP(Tkey, Tval, name, hashfunc) \
void HASHMAP_##name##_set(HASHMAP_##name* map, Tkey key, Tval value) { \
    if (map->capacity == 0 || ((double)(map->size + map->tombstones + 1) / map->capacity > 0.7)) { \
        size_t old_capacity = map->capacity; \
        HASHENTRY_##name* old_entries = map->entries; \
        map->capacity = map->capacity == 0 ? 2 : map->capacity*2; \
        map->entries = EZ_ALLOC(map->capacity, sizeof(HASHENTRY_##name)); \
        map->size = 0; \
        map->tombstones = 0; \
        for (size_t i = 0; i < old_capacity; i++) { \
            if (old_entries[i].used == 1) { \
                HASHMAP_##name##_set(map, old_entries[i].key, old_entries[i].value); \
            } \
        } \
        EZ_FREE(old_entries); \
    } \
    uint64_t hash = hashfunc(key); \
    size_t index = hash & (map->capacity - 1); \
    while (map->entries[index].used == 1 && \
           memcmp(&(map->entries[index].key), &key, sizeof(Tkey)) != 0) { \
        index = (index + 1) & (map->capacity - 1); \
    } \
    if (map->entries[index].used != 1) map->size++; \
    map->entries[index].key = key; \
    map->entries[index].value = value; \
    map->entries[index].used = 1; \
} \
\
BOOL HASHMAP_##name##_has(HASHMAP_##name* map, Tkey key) { \
    if (map->size == 0) return FALSE; \
    uint64_t hash = hashfunc(key); \
    size_t index = hash & (map->capacity - 1); \
    while (map->entries[index].used != 0) { \
        if (map->entries[index].used == 1 && \
            memcmp(&(map->entries[index].key), &key, sizeof(Tkey)) == 0) { \
            return TRUE; \
        } \
        index = (index + 1) & (map->capacity - 1); \
    } \
    return FALSE; \
} \
\
Tval HASHMAP_##name##_get(HASHMAP_##name* map, Tkey key) { \
    uint64_t hash = hashfunc(key); \
    size_t index = hash & (map->capacity - 1); \
    while (map->entries[index].used != 0) { \
        if (map->entries[index].used == 1 && \
            memcmp(&(map->entries[index].key), &key, sizeof(Tkey)) == 0) { \
            return map->entries[index].value; \
        } \
        index = (index + 1) & (map->capacity - 1); \
    } \
    EZ_FATAL("Could not get a value from a key that does not exist in the hashmap"); \
    Tval t; \
    return t; \
} \
\
void HASHMAP_##name##_remove(HASHMAP_##name* map, Tkey key) { \
    uint64_t hash = hashfunc(key); \
    size_t index = hash & (map->capacity - 1); \
    while (map->entries[index].used != 0) { \
        if (map->entries[index].used == 1 && \
            memcmp(&(map->entries[index].key), &key, sizeof(Tkey)) == 0) { \
            map->entries[index].used = 2; \
            map->tombstones++; \
            map->size--; \
            return; \
        } \
        index = (index + 1) & (map->capacity - 1); \
    } \
    EZ_FATAL("Could not remove a key that does not exist in the hashmap"); \
} \
\
void HASHMAP_##name##_clear(HASHMAP_##name* map) { \
    EZ_FREE(map->entries); \
    map->entries = NULL; \
    map->size = 0; \
    map->capacity = 0; \
}

#define DECLARE_PQUEUE(T) \
typedef struct { \
    T value; \
    float cost; \
} PQPAIR_##T; \
\
typedef struct { \
    PQPAIR_##T pair; \
    size_t index; \
} PQNODE_##T; \
\
typedef struct { \
    PQNODE_##T* list; \
    size_t* refs; \
    size_t size; \
    size_t capacity; \
} PQUEUE_##T; \
\
void PQUEUE_##T##_build(PQUEUE_##T* pq, PQPAIR_##T* values, size_t size); \
void PQUEUE_##T##_swap(PQUEUE_##T* pq, size_t i, size_t j); \
void PQUEUE_##T##_heapU(PQUEUE_##T* pq, size_t i); \
void PQUEUE_##T##_heapD(PQUEUE_##T* pq, size_t i); \
void PQUEUE_##T##_insert(PQUEUE_##T* pq, T value, float cost); \
T PQUEUE_##T##_pop(PQUEUE_##T* pq); \
T PQUEUE_##T##_top(PQUEUE_##T* pq); \
void PQUEUE_##T##_update(PQUEUE_##T* pq, size_t index, float newcost); \
void PQUEUE_##T##_clear(PQUEUE_##T* pq);

#define IMPL_PQUEUE(T) \
void PQUEUE_##T##_build(PQUEUE_##T* pq, PQPAIR_##T* values, size_t size) { \
    pq->size = size; \
    pq->capacity = size; \
    pq->list = EZ_ALLOC(size, sizeof(PQNODE_##T)); \
    pq->refs = EZ_ALLOC(size, sizeof(size_t*)); \
    for (size_t i = 0; i < size; i++) { \
        pq->list[i].pair = values[i]; \
        pq->list[i].index = i; \
        pq->refs[i] = i; \
    } \
    for (int64_t i = (size/2) - 1; i >= 0; i--) { \
        PQUEUE_##T##_heapD(pq, i); \
    } \
} \
\
void PQUEUE_##T##_swap(PQUEUE_##T* pq, size_t i, size_t j) { \
    size_t tmp = pq->refs[i]; \
    pq->refs[i] = pq->refs[j]; \
    pq->refs[j] = tmp; \
    pq->list[pq->refs[i]].index = i; \
    pq->list[pq->refs[j]].index = j; \
} \
\
void PQUEUE_##T##_heapU(PQUEUE_##T* pq, size_t i) { \
    while (i > 0) { \
        size_t parent = (i - 1) / 2; \
        if (pq->list[pq->refs[i]].pair.cost >= pq->list[pq->refs[parent]].pair.cost) break; \
        PQUEUE_##T##_swap(pq, i, parent); \
        i = parent; \
    } \
} \
\
void PQUEUE_##T##_heapD(PQUEUE_##T* pq, size_t i) { \
    while (TRUE) { \
        size_t smallest = i; \
        size_t left = i*2 + 1; \
        size_t right = i*2 + 2; \
        if (left < pq->size && pq->list[pq->refs[left]].pair.cost < pq->list[pq->refs[smallest]].pair.cost) smallest = left; \
        if (right < pq->size && pq->list[pq->refs[right]].pair.cost < pq->list[pq->refs[smallest]].pair.cost) smallest = right; \
        if (smallest == i) break; \
        PQUEUE_##T##_swap(pq, i, smallest); \
        i = smallest; \
    } \
} \
\
void PQUEUE_##T##_insert(PQUEUE_##T* pq, T value, float cost) { \
    if (pq->size >= pq->capacity) { \
        if (pq->capacity == 0) { \
            pq->capacity = 1; \
            pq->list = EZ_ALLOC(1, sizeof(PQNODE_##T)); \
            pq->refs = EZ_ALLOC(1, sizeof(size_t*)); \
        } \
        pq->capacity *= 2; \
        pq->list = EZ_REALLOC(pq->list, pq->capacity, sizeof(PQNODE_##T)); \
        pq->refs = EZ_REALLOC(pq->refs, pq->capacity, sizeof(size_t*)); \
    } \
    pq->list[pq->size] = (PQNODE_##T){(PQPAIR_##T){ value, cost }, pq->size}; \
    pq->refs[pq->size] = pq->size; \
    pq->size++; \
    PQUEUE_##T##_heapU(pq, pq->size - 1); \
} \
\
T PQUEUE_##T##_pop(PQUEUE_##T* pq) { \
    EZ_ASSERT(pq->size != 0, "Cannot pop off an empty PriorityQueue"); \
    T val = pq->list[pq->refs[0]].pair.value; \
    pq->size--; \
    if (pq->size > 0) { \
        pq->refs[0] = pq->refs[pq->size]; \
        pq->list[pq->refs[0]].index = 0; \
        PQUEUE_##T##_heapD(pq, 0); \
    } \
    return val; \
} \
\
T PQUEUE_##T##_top(PQUEUE_##T* pq) { \
    EZ_ASSERT(pq->size != 0, "Cannot get the top of an empty PriorityQueue"); \
    return pq->list[pq->refs[0]].pair.value; \
} \
\
void PQUEUE_##T##_update(PQUEUE_##T* pq, size_t index, float newcost) { \
    EZ_ASSERT(pq->list[index].index < pq->size && index < pq->capacity, "Cannot update an element that does not exist in the PriorityQueue"); \
    float oldcost = pq->list[index].pair.cost; \
    pq->list[index].pair.cost = newcost; \
    if (newcost < oldcost) PQUEUE_##T##_heapU(pq, pq->list[index].index); \
    else PQUEUE_##T##_heapD(pq, pq->list[index].index); \
} \
\
void PQUEUE_##T##_clear(PQUEUE_##T* pq) { \
    EZ_FREE(pq->list); \
    EZ_FREE(pq->refs); \
    pq->list = NULL; \
    pq->refs = NULL; \
    pq->size = 0; \
    pq->capacity = 0; \
}

#endif
