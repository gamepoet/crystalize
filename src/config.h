#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct crystalize_config_t crystalize_config_t;

crystalize_config_t* config_get();

#define crystalize_assert(expr, message) ((expr) ? true : (crystalize_assert_ex(__FILE__, __LINE__, __func__, #expr, message), false))

void crystalize_assert_ex(const char* file, int line, const char* func, const char* expression, const char* message);

#define crystalize_alloc(size) crystalize_alloc_ex(size, __FILE__, __LINE__, __func__)
#define crystalize_free(ptr) crystalize_free_ex(ptr, __FILE__, __LINE__, __func__)
#define crystalize_realloc(ptr, size) crystalize_realloc_ex(ptr, size, __FILE__, __LINE__, __func__)

void* crystalize_alloc_ex(size_t size, const char* file, int line, const char* func);
void crystalize_free_ex(void* ptr, const char* file, int line, const char* func);
void* crystalize_realloc_ex(void* ptr, size_t size, const char* file, int line, const char* func);

void default_assert_handler(const char* file, int line, const char* func, const char* expression, const char* message);
void* default_alloc_handler(size_t size, const char* file, int line, const char* func);
void default_free_handler(void* ptr, const char* file, int line, const char* func);

char* crystalize_strdup(const char* str);
