#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum crystalize_type_t {
  CRYSTALIZE_BOOL,
  CRYSTALIZE_CHAR,
  CRYSTALIZE_INT8,
  CRYSTALIZE_INT16,
  CRYSTALIZE_INT32,
  CRYSTALIZE_INT64,
  CRYSTALIZE_UINT8,
  CRYSTALIZE_UINT16,
  CRYSTALIZE_UINT32,
  CRYSTALIZE_UINT64,
  CRYSTALIZE_FLOAT,
  CRYSTALIZE_DOUBLE,
  CRYSTALIZE_STRUCT,
} crystalize_type_t;

typedef struct crystalize_schema_field_t {
  uint32_t name_id;         // name of this field
  uint32_t count;           // count >1 implies array
  uint32_t complex_type_id; // schema name id if type is complex
  uint16_t ref_field_index; // reference field index (for counted pointers)
  uint8_t type;             // type of this field
  bool pointer;             // is the type a pointer type?
} crystalize_schema_field_t;

typedef struct crystalize_schema_t {
  uint32_t name_id;
  uint32_t version;
  uint32_t field_count;
  const crystalize_schema_field_t* fields;
} crystalize_schema_t;

typedef void (*crystalize_assert_handler_t)(const char* file, int line, const char* func, const char* expression, const char* message);
typedef void* (*crystalize_alloc_handler_t)(size_t size, const char* file, int line, const char* func);
typedef void (*crystalize_free_handler_t)(void* ptr, const char* file, int line, const char* func);

typedef struct crystalize_config_t {
  // The handler to use when an assertion fails.
  crystalize_assert_handler_t assert_handler;

  // The handler to use when allocating memory.
  crystalize_alloc_handler_t alloc_handler;

  // The handler to use when freeing memory.
  crystalize_free_handler_t free_handler;
} crystalize_config_t;

void crystalize_config_init(crystalize_config_t* config);
void crystalize_init(const crystalize_config_t* config);
void crystalize_shutdown();

void crystalize_schema_field_init(crystalize_schema_field_t* field, const char* name, crystalize_type_t type, uint32_t count, uint32_t ref_type_name_id);
void crystalize_schema_init(crystalize_schema_t* schema, const char* name, uint32_t version, const crystalize_schema_field_t* fields, uint32_t field_count);

void crystalize_schema_add(const crystalize_schema_t* schema);
const crystalize_schema_t* crystalize_schema_get(uint32_t schema_name_id);

// Encodes the given data structure into a buffer using the given schema.
void crystalize_encode(uint32_t schema_name_id, const void* data, char** buf, uint32_t* buf_size);

// Decodes the buffer IN PLACE using the given expected schema.
void* crystalize_decode(uint32_t schema_name_id, char* buf, uint32_t buf_size);

#ifdef __cplusplus
}
#endif
