#ifndef EASYHASH_H
#define EASYHASH_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint64_t ez_hash_uint64_t(uint64_t x);

uint64_t ez_hash_size_t(size_t x);

uint64_t ez_hash_int(int x);

uint64_t ez_hash_float(float x);

uint64_t ez_hash_char(char x);

uint64_t ez_hash_uint32_t(uint32_t x);

uint64_t ez_hash_int32_t(int32_t x);

uint64_t ez_hash_int64_t(int64_t x);

uint64_t ez_hash_uint16_t(uint16_t x);

uint64_t ez_hash_int16_t(int16_t x);

uint64_t ez_hash_uint8_t(uint8_t x);

uint64_t ez_hash_int8_t(int8_t x);

#endif
