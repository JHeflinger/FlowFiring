#include <easyc.h>
#include <time.h>

double get_time() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

double arraylist_carousel(size_t arrs, size_t depth) {
    double time = get_time();
    ARRLIST_int* arrlists = EZ_ALLOC(arrs, sizeof(ARRLIST_int));
    for (size_t i = 0; i < depth; i++) {
        ARRLIST_int_add(&(arrlists[rand()%arrs]), 0xFFFFF);
        ARRLIST_int_clear(&(arrlists[rand()%arrs]));
    }
    for (size_t i = 0; i < arrs; i++) {
        ARRLIST_int_clear(&(arrlists[i]));
    }
    EZ_FREE(arrlists);
    return get_time() - time;
}

int main(int argc, const char** argv) {
    EZ_INFO("Starting benchmark suite...");
    srand(time(NULL));
    EZ_INFO("Benchmarking default memory tech using \"arraylist_carousel()\"");
    EZ_INFO("10x10         | %.6f ms", arraylist_carousel(10, 10));
    EZ_INFO("100x100       | %.6f ms", arraylist_carousel(100, 100));
    EZ_INFO("1000x1000     | %.6f ms", arraylist_carousel(1000, 1000));
    EZ_INFO("10000x10000   | %.6f ms", arraylist_carousel(10000, 10000));
    EZ_INFO("100000x100000 | %.6f ms", arraylist_carousel(100000, 100000));
    EZ_INFO("10x100000     | %.6f ms", arraylist_carousel(10, 100000));
}
