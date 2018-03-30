#include "hash.h"

#define FNV1A_PRIME 0x01000193ull
#define FNV1A_SEED 0x811c9dc5ull

uint32_t fnv1a(const char* buf, size_t size) {
  return fnv1a_with_seed(buf, size, FNV1A_SEED);
}

uint32_t fnv1a_with_seed(const char* buf, size_t size, uint32_t seed) {
  uint32_t hash = seed;
  const uint8_t* cur = (const uint8_t*)buf;
  const uint8_t* end = (const uint8_t*)buf + size;
  for (; cur < end; ++cur) {
    hash = (*cur ^ hash) * FNV1A_PRIME;
  }
  return hash;
}
