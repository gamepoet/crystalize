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

typedef enum crystalize_error_t {
  CRYSTALIZE_ERROR_NONE,
  CRYSTALIZE_ERROR_DATA_OFFSET_IS_INVALID,
  CRYSTALIZE_ERROR_ENDIAN_MISMATCH,
  CRYSTALIZE_ERROR_FILE_HEADER_MALFORMED,
  CRYSTALIZE_ERROR_FILE_VERSION_MISMATCH,
  CRYSTALIZE_ERROR_POINTER_INVALID,
  CRYSTALIZE_ERROR_POINTER_SIZE_MISMATCH,
  CRYSTALIZE_ERROR_POINTER_TABLE_OFFSET_IS_INVALID,
  CRYSTALIZE_ERROR_SCHEMA_ALREADY_ADDED,
  CRYSTALIZE_ERROR_SCHEMA_COUNT_FIELD_INVALID_TYPE,
  CRYSTALIZE_ERROR_SCHEMA_COUNT_FIELD_NOT_FOUND,
  CRYSTALIZE_ERROR_SCHEMA_IS_EMPTY,
  CRYSTALIZE_ERROR_SCHEMA_NOT_FOUND,
  CRYSTALIZE_ERROR_UNEXPECTED_EOF,
} crystalize_error_t;

typedef struct crystalize_schema_field_t {
  uint32_t name_id;             // hash(name) of this field
  uint32_t struct_name_id;      // the hash(name) for the struct (if type is struct)
  uint32_t struct_version;      // the version for the struct (if type is struct)
  uint32_t count;               // the fixed-size array length (zero implies pointer, >1 implies array)
  uint32_t count_field_name_id; // the index of the field in the same struct that provides a count (if pointer)
  uint8_t type;                 // the field's basic type (crystalize_type_t)
} crystalize_schema_field_t;

typedef struct crystalize_schema_t {
  uint32_t name_id;
  uint32_t version;
  uint32_t alignment;
  uint32_t field_count;
  const crystalize_schema_field_t* fields;
} crystalize_schema_t;

typedef struct crystalize_encode_result_t {
  char* buf;
  uint32_t buf_size;
  crystalize_error_t error;
  char* error_message;
} crystalize_encode_result_t;

typedef struct crystalize_decode_result_t {
  crystalize_error_t error;
} crystalize_decode_result_t;

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

void crystalize_schema_field_init_scalar(crystalize_schema_field_t* field,
                                         const char* name,
                                         crystalize_type_t type,
                                         uint32_t count);
void crystalize_schema_field_init_struct(crystalize_schema_field_t* field,
                                         const char* name,
                                         const crystalize_schema_t* schema,
                                         uint32_t count);
void crystalize_schema_field_init_counted_scalar(crystalize_schema_field_t* field,
                                                 const char* name,
                                                 crystalize_type_t type,
                                                 const char* count_field_name);
void crystalize_schema_field_init_counted_struct(crystalize_schema_field_t* field,
                                                 const char* name,
                                                 const crystalize_schema_t* schema,
                                                 const char* count_field_name);
void crystalize_schema_init(crystalize_schema_t* schema,
                            const char* name,
                            uint32_t version,
                            uint32_t alignment,
                            const crystalize_schema_field_t* fields,
                            uint32_t field_count);

crystalize_error_t crystalize_schema_add(const crystalize_schema_t* schema);
const crystalize_schema_t* crystalize_schema_get(uint32_t schema_name_id, uint32_t schema_version);

// Encodes the given data structure into a buffer using the given schema.
void crystalize_encode(uint32_t schema_name_id, uint32_t schema_version, const void* data, crystalize_encode_result_t* result);
void crystalize_encode_result_free(crystalize_encode_result_t* result);

// Decodes the buffer IN PLACE using the given expected schema.
void* crystalize_decode(uint32_t schema_name_id, uint32_t schema_version, char* buf, uint32_t buf_size, crystalize_decode_result_t* result);

#ifdef __cplusplus
}
#endif
