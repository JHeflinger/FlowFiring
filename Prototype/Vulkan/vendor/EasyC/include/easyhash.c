#include "easyhash.h"

uint64_t ez_hash_uint64_t(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

uint64_t ez_hash_size_t(size_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_int(int x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_float(float x) {
    uint64_t y = 0;
    memcpy(&y, &x, sizeof(x));
    return ez_hash_uint64_t((uint64_t)y);
}

uint64_t ez_hash_char(char x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_uint32_t(uint32_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_int32_t(int32_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_int64_t(int64_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_uint16_t(uint16_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_int16_t(int16_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_uint8_t(uint8_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}

uint64_t ez_hash_int8_t(int8_t x) {
    return ez_hash_uint64_t((uint64_t)x);
}
