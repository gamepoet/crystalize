#include "crystalize.h"
#include "config.h"
#include "encoder.h"
#include "hash.h"
#include <assert.h>
#include <stdalign.h>
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

static int s_schemas_capacity;
static int s_schemas_count;
static crystalize_schema_t* s_schemas;

crystalize_schema_t s_schema_schema;       // the schema for crystalize_schema_t
static crystalize_schema_t s_schema_schema_field; // the schema for crystalize_schema_field_t
static crystalize_schema_field_t s_schema_schema_fields[5];
static crystalize_schema_field_t s_schema_schema_field_fields[5];

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
  config->alloc_handler = &default_alloc_handler;
  config->free_handler = &default_free_handler;
}

void crystalize_init(const crystalize_config_t* config) {
  if (config != NULL) {
    *config_get() = *config;
  }
  else {
    crystalize_config_init(config_get());
  }

  s_schemas_capacity = 0;
  s_schemas_count = 0;
  s_schemas = NULL;

  crystalize_schema_init(&s_schema_schema_field, "__crystalize_schema_field", 0, alignof(crystalize_schema_field_t), s_schema_schema_field_fields, 5);
  crystalize_schema_field_init_scalar(s_schema_schema_field_fields + 0, "name_id", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_field_fields + 1, "struct_name_id", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_field_fields + 2, "count", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_field_fields + 3, "count_field_name_id", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_field_fields + 4, "type", CRYSTALIZE_UINT8, 1);
  crystalize_schema_add(&s_schema_schema_field);

  crystalize_schema_init(&s_schema_schema, "__crystalize_schema", 0, alignof(crystalize_schema_t), s_schema_schema_fields, 5);
  crystalize_schema_field_init_scalar(s_schema_schema_fields + 0, "name_id", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_fields + 1, "version", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_fields + 2, "alignment", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_scalar(s_schema_schema_fields + 3, "field_count", CRYSTALIZE_UINT32, 1);
  crystalize_schema_field_init_counted_struct(s_schema_schema_fields + 4, "fields", &s_schema_schema_field, "field_count");
  crystalize_schema_add(&s_schema_schema);
}

void crystalize_shutdown() {
  free(s_schemas);
  s_schemas = NULL;
  s_schemas_count = 0;
  s_schemas_capacity = 0;
}

void crystalize_schema_field_init_scalar(crystalize_schema_field_t* field,
                                  const char* name,
                                  crystalize_type_t type,
                                  uint32_t count) {
  crystalize_assert(field != NULL, "field cannot be null");
  crystalize_assert(name != NULL, "name cannot be null");
  crystalize_assert(count > 0, "cannot have zero count");
  field->name_id = fnv1a(name, strlen(name));
  field->struct_name_id = 0;
  field->count = count;
  field->count_field_name_id = 0;
  field->type = type;
}

void crystalize_schema_field_init_struct(crystalize_schema_field_t* field,
                                         const char* name,
                                         const crystalize_schema_t* schema,
                                         uint32_t count) {
  crystalize_assert(field != NULL, "field cannot be null");
  crystalize_assert(name != NULL, "name cannot be null");
  crystalize_assert(schema != NULL, "schema cannot be null");
  crystalize_assert(count > 0, "cannot have zero count");
  field->name_id = fnv1a(name, strlen(name));
  field->struct_name_id = schema->name_id;
  field->count = count;
  field->count_field_name_id = 0;
  field->type = CRYSTALIZE_STRUCT;
}

void crystalize_schema_field_init_counted_scalar(crystalize_schema_field_t* field,
                                                 const char* name,
                                                 crystalize_type_t type,
                                                 const char* count_field_name)
{
  crystalize_assert(field != NULL, "field cannot be null");
  crystalize_assert(name != NULL, "name cannot be null");
  crystalize_assert(count_field_name != NULL, "count_field_name cannot be null");
  field->name_id = fnv1a(name, strlen(name));
  field->struct_name_id = 0;
  field->count = 0;
  field->count_field_name_id = fnv1a(count_field_name, strlen(count_field_name));
  field->type = type;
}

void crystalize_schema_field_init_counted_struct(crystalize_schema_field_t* field,
                                  const char* name,
                                  const crystalize_schema_t* schema,
                                  const char* count_field_name) {
  crystalize_assert(field != NULL, "field cannot be null");
  crystalize_assert(name != NULL, "name cannot be null");
  crystalize_assert(schema != NULL, "schema cannot be null");
  crystalize_assert(count_field_name != NULL, "name cannot be null");
  field->name_id = fnv1a(name, strlen(name));
  field->struct_name_id = schema->name_id;
  field->count = 0;
  field->count_field_name_id = fnv1a(count_field_name, strlen(count_field_name));
  field->type = CRYSTALIZE_STRUCT;
}

void crystalize_schema_init(crystalize_schema_t* schema,
                            const char* name,
                            uint32_t version,
                            uint32_t alignment,
                            const crystalize_schema_field_t* fields,
                            uint32_t field_count) {
  crystalize_assert(schema != NULL, "schema cannot be null");
  schema->name_id = fnv1a(name, strlen(name));
  schema->version = version;
  schema->alignment = alignment;
  schema->fields = fields;
  schema->field_count = field_count;
}

void crystalize_schema_add(const crystalize_schema_t* schema) {
  crystalize_assert(-1 == schema_find(schema->name_id), "schema is already registered");
  crystalize_assert(schema->field_count > 0, "schema cannot represent an empty struct");
  crystalize_assert(schema->fields != NULL, "schema fields cannot be NULL");
  // verify all the fields have unique names
  for (uint32_t field_index = 0; field_index < schema->field_count - 1; ++field_index) {
    for (uint32_t other_index = field_index + 1; other_index < schema->field_count; ++other_index) {
      crystalize_assert(schema->fields[field_index].name_id != schema->fields[other_index].name_id, "schema has two fields with the same name");
    }
  }

  if (s_schemas_count >= s_schemas_capacity) {
    s_schemas_capacity += 128;
    s_schemas = (crystalize_schema_t*)realloc(s_schemas, s_schemas_capacity * sizeof(crystalize_schema_t));
  }
  s_schemas[s_schemas_count] = *schema;
  ++s_schemas_count;
}

const crystalize_schema_t* crystalize_schema_get(uint32_t schema_name_id) {
  const int index = schema_find(schema_name_id);
  if (index == -1) {
    return NULL;
  }
  else {
    return s_schemas + index;
  }
}

void crystalize_encode(uint32_t schema_name_id, const void* data, crystalize_encode_result_t* result) {
  crystalize_assert(result, "result cannot be null");
  const int schema_index = schema_find(schema_name_id);
  crystalize_assert(schema_index != -1, "schema not found");
  const crystalize_schema_t* schema = s_schemas + schema_index;

  result->buf = NULL;
  result->buf_size = 0;
  result->error = CRYSTALIZE_ERROR_NONE;
  result->error_message = NULL;
  encoder_encode(schema, data, result);
}

void crystalize_encode_result_free(crystalize_encode_result_t* result) {
  crystalize_assert(result, "result cannot be null");
  if (result->buf != NULL) {
    crystalize_free(result->buf);
    result->buf = NULL;
  }
  if (result->error_message != NULL) {
    crystalize_free(result->error_message);
    result->error_message = NULL;
  }
  result->buf_size = 0;
  result->error = CRYSTALIZE_ERROR_NONE;
}

void* crystalize_decode(uint32_t schema_name_id, char* buf, uint32_t buf_size) {
  const int schema_index = schema_find(schema_name_id);
  crystalize_assert(schema_index != -1, "schema not found");
  const crystalize_schema_t* schema = s_schemas + schema_index;

  return encoder_decode(schema, buf, buf_size);
}
