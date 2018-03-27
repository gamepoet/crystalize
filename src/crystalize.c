#include "crystalize.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// typedef struct sda_header_t {
//   uint32_t capacity;
//   uint32_t count;
// } sda_header_t;
//
// #define sda__header(a) ((sda_header_t*)((char*)(a) - sizeof(sda_header_t)))
// #define sda__capacity(a) sda_header(a)->capacity
// #define sda__count(a) sda__header(a)->count
//
// // frees the allocation for the array
// #define sda_free(a) ((a) ? free(sda__header(a)), 0 : 0)
//
// // gets the number of items currently stored in the array
// #define sda_count(a) ((a) ? sda__count(a) : 0)
//
// // gets the max number of items that can be stored in the array without additional allocation
// #define sda_capacity(a) ((a) ? sda__capacity(a) : 0)
//
// // ensures there is enough space to hold N elements, reallocating if necessary
// #define sda_reserve(a, n) (((a) == 0) || (sda__count(a) + (n) > sda__capacity(a))) ? sda__grow(a, sda)
//
// // pushes an item onto the end of the array
// #define sda_push(a, item) sda__maybe_grow(a, 1), (a)[sda__count(a)++] = (item)
//
// tests if the array contains an item
// #define sda_contains(a, item) \
//   for (int index = 0; index < )

static crystalize_config_t s_config;

static int s_schemas_capacity;
static int s_schemas_count;
static crystalize_schema_t* s_schemas;

static void
default_assert_handler(const char* file, int line, const char* func, const char* expression, const char* message) {
  fprintf(stderr, "ASSERT FAILURE: %s\n%s\nfile: %s\nline: %d\nfunc: %s\n", expression, message, file, line, func);
  exit(EXIT_FAILURE);
}

#define crystalize_assert(expr, message)                                                                               \
  ((expr) ? true : (crystalize_assert_ex(__FILE__, __LINE__, __func__, #expr, message), false))

static void
crystalize_assert_ex(const char* file, int line, const char* func, const char* expression, const char* message) {
  s_config.assert_handler(file, line, func, expression, message);
}

#define FNV1A_PRIME 0x01000193ull
#define FNV1A_SEED 0x811c9dc5ull

static uint32_t fnv1a(const char* buf, size_t size, uint32_t hash) {
  const uint8_t* cur = (const uint8_t*)buf;
  const uint8_t* end = (const uint8_t*)buf + size;
  for (; cur < end; ++cur) {
    hash = (*cur ^ hash) * FNV1A_PRIME;
  }
  return hash;
}

static int schema_find(uint32_t name_id) {
  for (int index = 0; index < s_schemas_count; ++index) {
    if (s_schemas[index].name_id == name_id) {
      return index;
    }
  }
  return -1;
}

void crystalize_config_init(crystalize_config_t* config) {
  if (config == NULL) {
    return;
  }

  config->assert_handler = &default_assert_handler;
}

void crystalize_init(const crystalize_config_t* config) {
  if (config != NULL) {
    s_config = *config;
  }
  else {
    crystalize_config_init(&s_config);
  }

  s_schemas_capacity = 0;
  s_schemas_count = 0;
  s_schemas = NULL;
}

void crystalize_shutdown() {
  free(s_schemas);
  s_schemas = NULL;
  s_schemas_count = 0;
  s_schemas_capacity = 0;
}

void crystalize_schema_field_init(crystalize_schema_field_t* field,
                                  const char* name,
                                  crystalize_type_t type,
                                  uint32_t count) {
  crystalize_assert(field != NULL, "field cannot be null");
  field->name_id = fnv1a(name, strlen(name), FNV1A_SEED);
  field->count = count;
  field->type = type;
}

void crystalize_schema_init(crystalize_schema_t* schema, const char* name, uint32_t version, const crystalize_schema_field_t* fields, uint32_t field_count)
{
  crystalize_assert(schema != NULL, "schema cannot be null");
  schema->name_id = fnv1a(name, strlen(name), FNV1A_SEED);
  schema->version = version;
  schema->fields = fields;
  schema->field_count = field_count;
}

void crystalize_schema_add(const crystalize_schema_t* schema) {
  crystalize_assert(-1 == schema_find(schema->name_id), "schema is already registered");
  if (s_schemas_count >= s_schemas_capacity) {
    s_schemas_capacity += 128;
    s_schemas = (crystalize_schema_t*)realloc(s_schemas, s_schemas_capacity * sizeof(crystalize_schema_t));
  }
  s_schemas[s_schemas_count] = *schema;
  ++s_schemas_count;
}

const crystalize_schema_t* crystalize_schema_get(uint32_t schema_name_id) {
  int index = schema_find(schema_name_id);
  if (index == -1) {
    return NULL;
  }
  else {
    return s_schemas + index;
  }
}
