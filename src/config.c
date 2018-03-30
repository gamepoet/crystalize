#include "config.h"
#include "crystalize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static crystalize_config_t s_config;

crystalize_config_t* config_get() {
  return &s_config;
}

void config_set(const crystalize_config_t* config) {
  s_config = *config;
}

void crystalize_assert_ex(const char* file, int line, const char* func, const char* expression, const char* message) {
  s_config.assert_handler(file, line, func, expression, message);
}

void* crystalize_alloc_ex(size_t size, const char* file, int line, const char* func) {
  return s_config.alloc_handler(size, file, line, func);
}

void crystalize_free_ex(void* ptr, const char* file, int line, const char* func) {
  s_config.free_handler(ptr, file, line, func);
}

void* crystalize_realloc_ex(void* ptr, size_t size, const char* file, int line, const char* func) {
  void* new_ptr = crystalize_alloc_ex(size, file, line, func);
  crystalize_assert(new_ptr != NULL, "allocation failed");
  if (ptr != NULL) {
    memmove(new_ptr, ptr, size);
    crystalize_free_ex(ptr, __FILE__, __LINE__, __func__);
  }
  return new_ptr;
}

void default_assert_handler(const char* file, int line, const char* func, const char* expression, const char* message) {
  fprintf(stderr, "ASSERT FAILURE: %s\n%s\nfile: %s\nline: %d\nfunc: %s\n", expression, message, file, line, func);
  exit(EXIT_FAILURE);
}

void* default_alloc_handler(size_t size, const char* file, int line, const char* func) {
  return malloc(size);
}

void default_free_handler(void* ptr, const char* file, int line, const char* func) {
  free(ptr);
}
