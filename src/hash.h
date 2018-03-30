#pragma once
#include <stddef.h>
#include <stdint.h>

uint32_t fnv1a(const char* buf, size_t size);
uint32_t fnv1a_with_seed(const char* buf, size_t size, uint32_t seed);
