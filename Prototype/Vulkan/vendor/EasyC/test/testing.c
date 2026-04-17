#include <easyc.h>
#include <math.h>

int passed_tests = 0;
int total_tests = 0;

#define EZTEST(condition, name) { \
	total_tests++; \
	if (condition) { \
		printf("%d. %s - %s[PASSED]%s\n", (int)total_tests, name, EZ_GREEN, EZ_RESET); \
		passed_tests++; \
	} else { \
		printf("%d. %s - %s[FAILED]%s\n", (int)total_tests, name, EZ_RED, EZ_RESET); \
	} \
}

DECLARE_ARRLIST_NAMED(intPtr, int*);
IMPL_ARRLIST_NAMED(intPtr, int*);
DECLARE_HASHMAP(int, char, int2char);
IMPL_HASHMAP(int, char, int2char, ez_hash_int);

typedef struct {
	int* sum;
	int a;
	int b;
	EZ_MUTEX mutex;
} Params;

typedef struct {
	int32_t a;
	char b;
	float c;
	uint32_t d;
} Packet;

EZ_THREAD_RETURN_TYPE thread_function(EZ_THREAD_PARAMETER_TYPE params) {
	Params* p = (Params*)params;
	*(p->sum) = p->a + p->b;
	return 0;
}

EZ_THREAD_RETURN_TYPE unsafe_function(EZ_THREAD_PARAMETER_TYPE params) {
	Params* p = (Params*)params;
	EZ_LOCK_MUTEX(p->mutex);
	*(p->sum) = *(p->sum) + 1;
	EZ_RELEASE_MUTEX(p->mutex);
	return 0;
}

int main() {
	// easybool tests
	EZTEST(TRUE == 1, "Truth bool");
	EZTEST(FALSE == 0, "False bool");
	EZTEST(sizeof(BOOL) == sizeof(int), "Bool size");
	
	// easymemory tests
	void* obj = EZ_ALLOC(1, 100);
	EZTEST(obj != NULL, "Successful allocation");
	EZTEST(EZ_ALLOCATED() == 100 + sizeof(size_t), "Allocated size");
    obj = EZ_REALLOC(obj, 1, 10);
    EZTEST(EZ_ALLOCATED() == 10 + sizeof(size_t), "Reallocated smaller size");
    obj = EZ_REALLOC(obj, 1, 1000);
    EZTEST(EZ_ALLOCATED() == 1000 + sizeof(size_t), "Reallocated larger size");
    void* robj = EZ_ALLOC(1, 1000);
    memset(robj, 0, 1000);
    EZTEST(memcmp(obj, robj, 1000) == 0, "Preset reallocation to zero");
    memset(obj, 1, 1000);
    memset(robj, 1, 500);
    obj = EZ_REALLOC(obj, 1, 500);
    EZTEST(memcmp(obj, robj, 500) == 0, "Reallocation unchanged data");
    EZ_FREE(robj);
    obj = EZ_REALLOC(obj, 5, 10);
    EZTEST(EZ_ALLOCATED() == 50 + sizeof(size_t), "Multiple reallocation");
	EZ_FREE(obj);
	EZTEST(EZ_ALLOCATED() == 0, "Free memory");
	obj = EZ_ALLOC(2, 100);
	EZTEST(EZ_ALLOCATED() == 200 + sizeof(size_t), "Multiplicative allocation");
	void* obj2 = EZ_ALLOC(2, 100);
	memset(obj2, 0, 200);
	EZTEST(memcmp(obj, obj2, 200) == 0, "Preset to zero");
	EZ_FREE(obj);
	EZ_FREE(obj2);

	// easyobjects tests
	size_t before_eo_tests = EZ_ALLOCATED();
	ARRLIST_int list = { 0 };
	EZTEST(list.size == 0, "Empty list");
	EZTEST(list.maxsize == 0, "Empty list capacity");
	ARRLIST_int_add(&list, 2);
	ARRLIST_int_add(&list, 6);
	ARRLIST_int_add(&list, 10);
	ARRLIST_int_insert(&list, 4, 1);
	ARRLIST_int_insert(&list, 8, 3);
	EZTEST(list.size == 5, "Filled list");
	EZTEST(list.maxsize == 8, "List capacity");
	EZTEST(ARRLIST_int_has(&list, 10), "List contains");
	EZTEST(!ARRLIST_int_has(&list, 5), "List does not contains");
	ARRLIST_int_remove(&list, 4);
	EZTEST(!ARRLIST_int_has(&list, 10), "List removal");
	EZTEST(list.size == 4, "List removed size");
	EZTEST(list.maxsize == 8, "List maintained capacity");
	EZTEST(ARRLIST_int_get(&list, 2) == 6, "List get");
	ARRLIST_int_clear(&list);
	EZTEST(list.size == 0, "List clear size");
	EZTEST(list.maxsize == 0, "List clear capacity");
    ARRLIST_int_zero(&list, 123456);
    EZTEST(list.size == 123456, "List zero size");
    EZTEST(list.maxsize == 123456, "List zero maxsize");
    int success = 1;
    for (int i = 0; i < 123456; i++) {
        if (list.data[i] != 0) success = 0;
    }
    EZTEST(success == 1, "List zero zero'd");
    ARRLIST_int_clear(&list);
	ARRLIST_intPtr nlist = { 0 };
	EZTEST(nlist.size == 0, "Empty named list");
	EZTEST(nlist.maxsize == 0, "Empty named list capacity");
	int i = 1;
	int j = 3;
	int k = 5;
	int z = 1000;
	ARRLIST_intPtr_add(&nlist, &i);
	ARRLIST_intPtr_add(&nlist, &j);
	ARRLIST_intPtr_add(&nlist, &k);
	EZTEST(nlist.size == 3, "Filled named list");
	EZTEST(nlist.maxsize == 4, "Named list capacity");
	EZTEST(ARRLIST_intPtr_has(&nlist, &j), "Named list contains");
	EZTEST(!ARRLIST_intPtr_has(&nlist, &z), "Named list does not contains");
	ARRLIST_intPtr_remove(&nlist, 1);
	EZTEST(!ARRLIST_intPtr_has(&nlist, &j), "Named list removal");
	EZTEST(nlist.size == 2, "Named list removed size");
	EZTEST(nlist.maxsize == 4, "Named list maintained capacity");
	EZTEST(ARRLIST_intPtr_get(&nlist, 1) == &k, "Named list get");
	ARRLIST_intPtr_clear(&nlist);
	EZTEST(nlist.size == 0, "Named list clear size");
	EZTEST(nlist.maxsize == 0, "Named list clear capacity");
    ARRLIST_intPtr_zero(&nlist, 123456);
    EZTEST(nlist.size == 123456, "Named list zero size");
    EZTEST(nlist.maxsize == 123456, "Named list zero maxsize");
    success = 1;
    for (int i = 0; i < 123456; i++) {
        if (nlist.data[i] != NULL) success = 0;
    }
    EZTEST(success == 1, "Named list zero zero'd");
    ARRLIST_intPtr_clear(&nlist);
    HASHMAP_int2char hm = { 0 };
    EZTEST(hm.size == 0, "Empty hashmap");
    EZTEST(hm.capacity == 0, "Empty hashmap capacity");
    EZTEST(!HASHMAP_int2char_has(&hm, 99), "Empty hashmap has");
    HASHMAP_int2char_set(&hm, 99, 'a');
    HASHMAP_int2char_set(&hm, 69, 'b');
    EZTEST(!HASHMAP_int2char_has(&hm, 67), "Non-empty hashmap has on non-existent key");
    HASHMAP_int2char_set(&hm, 67, 'c');
    HASHMAP_int2char_set(&hm, 21, 'd');
    EZTEST(hm.size == 4, "Filled hashmap size");
    EZTEST(hm.capacity == 8, "Filled hashmap capacity");
    success = 
        HASHMAP_int2char_has(&hm, 99) &&
        HASHMAP_int2char_has(&hm, 69) &&
        HASHMAP_int2char_has(&hm, 67) &&
        HASHMAP_int2char_has(&hm, 21);
    EZTEST(success, "Filled hashmap has");
    success =
        HASHMAP_int2char_has(&hm, 1) ||
        HASHMAP_int2char_has(&hm, 98) ||
        HASHMAP_int2char_has(&hm, 100) ||
        HASHMAP_int2char_has(&hm, 2);
    EZTEST(!success, "Filled hashmap (does not) has");
    EZTEST(HASHMAP_int2char_get(&hm, 99) == 'a', "Filled hashmap get 1");
    EZTEST(HASHMAP_int2char_get(&hm, 69) == 'b', "Filled hashmap get 2");
    EZTEST(HASHMAP_int2char_get(&hm, 67) == 'c', "Filled hashmap get 3");
    EZTEST(HASHMAP_int2char_get(&hm, 21) == 'd', "Filled hashmap get 4");
    HASHMAP_int2char_set(&hm, 99, 'z');
    EZTEST(HASHMAP_int2char_get(&hm, 99) == 'z', "Hashmap replace key-value pair");
    HASHMAP_int2char_remove(&hm, 99);
    EZTEST(!HASHMAP_int2char_has(&hm, 99), "Filled hashmap remove 1");
    HASHMAP_int2char_remove(&hm, 69);
    EZTEST(!HASHMAP_int2char_has(&hm, 69), "Filled hashmap remove 2");
    HASHMAP_int2char_remove(&hm, 67);
    EZTEST(!HASHMAP_int2char_has(&hm, 67), "Filled hashmap remove 3");
    HASHMAP_int2char_remove(&hm, 21);
    EZTEST(!HASHMAP_int2char_has(&hm, 21), "Filled hashmap remove 4");
    EZTEST(hm.size == 0, "Cleared out hashmap");
    HASHMAP_int2char_clear(&hm);
    EZTEST(hm.capacity == 0, "Cleared out hashmap capacity");
    PQUEUE_int pqi = { 0 };
    EZTEST(pqi.size == 0, "Empty priority queue");
    EZTEST(pqi.capacity == 0, "Empty priority queue capacity");
    PQUEUE_int_insert(&pqi, 1, 9.0f);
    PQUEUE_int_insert(&pqi, 2, 8.0f);
    PQUEUE_int_insert(&pqi, 3, 7.0f);
    PQUEUE_int_insert(&pqi, 4, 6.0f);
    PQUEUE_int_insert(&pqi, 5, 5.0f);
    PQUEUE_int_insert(&pqi, 6, 4.0f);
    PQUEUE_int_insert(&pqi, 7, 3.0f);
    PQUEUE_int_insert(&pqi, 8, 2.0f);
    PQUEUE_int_insert(&pqi, 9, 1.0f);
    PQUEUE_int_insert(&pqi, 10, 0.0f);
    EZTEST(pqi.size == 10, "Filled priority queue size");
    EZTEST(pqi.capacity == 16, "Filled priority queue capacity");
    size_t t = pqi.size;
    int curr = 10;
    int topv = 1;
    for (size_t i = 0; i < t; i++) {
        topv |= (PQUEUE_int_top(&pqi) == curr);
        int new = PQUEUE_int_pop(&pqi);
        if (new != curr) {
            curr = -1;
            break;
        }
        curr--;
    }
    EZTEST(topv, "Priority queue top");
    EZTEST(curr == 0, "Priority queue insert and pop");
    EZTEST(pqi.size == 0, "Popped out priority queue size");
    PQUEUE_int_clear(&pqi);
    PQPAIR_int buildarr[] = {
        {92, 8.0f}, {98, 2.0f}, {90, 10.0f}, {94, 6.0f}, {97, 3.0f},
        {96, 4.0f}, {93, 7.0f}, {99, 1.0f}, {91, 9.0f}, {95, 5.0f}
    };
    int verifyarr[] = { 93, 94, 91, 99, 98, 97, 96, 95, 92, 90 };
    PQUEUE_int_build(&pqi, buildarr, 10);
    EZTEST(pqi.size == 10, "Built priority queue size");
    EZTEST(pqi.capacity == 10, "Built priority queue capacity");
    success = 1;
    for (size_t i = 0; i < pqi.size; i++)
        success &= PQUEUE_int_pop(&pqi) == 99 - (int)i;
    EZTEST(success, "Built priority queue order");
    PQUEUE_int_clear(&pqi);
    EZTEST(pqi.size == 0, "Cleared priority queue size");
    EZTEST(pqi.capacity == 0, "Cleared priority queue capacity");
    PQUEUE_int_build(&pqi, buildarr, 10);
    PQUEUE_int_update(&pqi, 3, -99.0f);
    PQUEUE_int_update(&pqi, 6, -99.001f);
    PQUEUE_int_update(&pqi, 8, -11.9f);
    size_t pqisize = pqi.size;
    success = 1;
    for (size_t i = 0; i < pqisize; i++)
        success &= PQUEUE_int_pop(&pqi) == verifyarr[i];
    EZTEST(success, "Update priority queue");
    PQUEUE_int_clear(&pqi);
	EZTEST(before_eo_tests == EZ_ALLOCATED(), "EasyObjects memory leak");
	
	// easythreads tests
	int sum = 0;
	EZ_MUTEX mutex;
	EZ_CREATE_MUTEX(mutex);
	Params par = { &sum, 10, 59, mutex };
	EZ_THREAD safe_thread;
	EZ_CREATE_THREAD(safe_thread, thread_function, &par);
	EZ_WAIT_THREAD(safe_thread);
	EZTEST(sum == 69, "Safe thread");
	sum = 0;
	EZ_THREAD unsafe_threads[1234];
	for (int i = 0; i < 1234; i++) {
		EZ_CREATE_THREAD(unsafe_threads[i], unsafe_function, &par);
	}
	for (int i = 0; i < 1234; i++) {
		EZ_WAIT_THREAD(unsafe_threads[i]);
	}
	EZTEST(sum == 1234, "Unsafe threads");
	
	// easynet tests
	size_t en_allocated = EZ_ALLOCATED();
	ez_Buffer* buf = EZ_GENERATE_BUFFER(1024);
	Packet packet = { -123, 'z', 69.0f, UINT32_MAX };
	EZTEST(buf->max_length == 1024, "Generate buffer max size");
	EZTEST(buf->current_length == 0, "Empty buffer");
	EZTEST(EZ_RECORD_BUFFER(buf, &packet), "Record buffer success");
	EZTEST(buf->current_length == sizeof(Packet), "Record buffer size");
	int count = 1023;
	while (buf->bytes[count] == 0) {
		count--;
	}
	EZTEST(count + 1 == sizeof(Packet), "Real buffer size");
	Packet affirm = { 0 };
	EZTEST(EZ_TRANSLATE_BUFFER(buf, &affirm), "Translate buffer success");
	EZTEST(affirm.a == -123 && affirm.b == 'z' && affirm.c == 69.0f && affirm.d == UINT32_MAX, "Translate buffer data");
	EZTEST(EZ_INIT_NETWORK(), "Initialize network");
	ez_Server* server = EZ_GENERATE_SERVER();
	ez_Client* client = EZ_GENERATE_CLIENT();
	Ipv4 address = {{127, 0, 0, 1}};
	ez_Buffer* retbuf = EZ_GENERATE_BUFFER(1024);
	EZTEST(EZ_OPEN_SERVER(server, 55000), "Open server");
	EZTEST(EZ_CONNECT_CLIENT(client, address, 55000), "Connect client");
	ez_Connection* connection = EZ_SERVER_ACCEPT(server);
	EZTEST(connection != NULL, "Accepted client");
	EZTEST(!EZ_SERVER_ASK(connection, retbuf), "Non blocking server recieve failure");
	EZTEST(EZ_CLIENT_SEND(client, buf), "Client send");
	EZTEST(EZ_SERVER_ASK(connection, retbuf), "Non blocking server recieve success");
	EZTEST(memcmp(buf->bytes, retbuf->bytes, buf->max_length) == 0, "Successful byte transfer 1");
	memset(retbuf->bytes, 0, retbuf->max_length);
	EZ_CLIENT_SEND(client, buf);
	EZTEST(EZ_SERVER_RECIEVE(connection, retbuf), "Blocking server recieve");
	EZTEST(memcmp(buf->bytes, retbuf->bytes, buf->max_length) == 0, "Successful byte transfer 2");
	memset(retbuf->bytes, 0, retbuf->max_length);
	EZTEST(!EZ_CLIENT_ASK(client, retbuf), "Non blocking client recieve failure");
	EZTEST(EZ_SERVER_SEND(connection, buf), "Server send");
	EZTEST(EZ_CLIENT_ASK(client, retbuf), "Non blocking client recieve success");
	EZTEST(memcmp(buf->bytes, retbuf->bytes, buf->max_length) == 0, "Successful byte transfer 3");
	memset(retbuf->bytes, 0, retbuf->max_length);
	EZ_SERVER_SEND(connection, buf);
	EZTEST(EZ_CLIENT_RECIEVE(client, retbuf), "Blocking client recieve");
	EZTEST(memcmp(buf->bytes, retbuf->bytes, buf->max_length) == 0, "Successful byte transfer 4");
	EZTEST(EZ_CLOSE_CONNECTION(connection), "Close connection");
	EZTEST(EZ_DISCONNECT_CLIENT(client), "Disconnect client");
	EZTEST(EZ_CLOSE_SERVER(server), "Close server");
	EZTEST(EZ_CLEAN_NETWORK(), "Clean network");
	EZ_CLEAN_BUFFER(buf);
	EZ_CLEAN_BUFFER(retbuf);
	EZTEST(en_allocated == EZ_ALLOCATED(), "EasyNet memory leak");

	// easymath tests
	EZTEST(EZ_CLAMP(-99, -60, 100) == -60, "EasyMath clamp to min");
	EZTEST(EZ_CLAMP(999.0, 0.0, 100.0) == 100.0, "EasyMath clamp to max");
	EZTEST(EZ_CLAMP(100.0f, 0.0f, 200.0f) == 100.0f, "EasyMath clamp to none");
	EZTEST(EZ_DISTANCE(0, 25, 0, 0) == 25.0f, "EasyMath distance 1D");
	EZTEST(EZ_DISTANCE(3, 3, -3, -3) == (float)sqrt(72), "EasyMath distance square");
	EZTEST(EZ_DISTANCE(1, 10, 3, 4) == (float)sqrt(40), "EasyMath distance 2D");

	printf("\nTest suite results: %s%d%s/%d tests passed\n", 
		passed_tests == total_tests ? EZ_GREEN : EZ_RED, passed_tests, EZ_RESET, total_tests);
	return 0;
}
