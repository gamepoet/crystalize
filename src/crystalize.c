#include "crystalize.h"
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

#define ALIGN(x, align) (((x) + (align) - 1) & (~((align) - 1)))
#define ALIGN_PTR(T, p, align) ((T*)ALIGN((uintptr_t)(p), (uintptr_t)align))

static crystalize_config_t s_config;

static int s_schemas_capacity;
static int s_schemas_count;
static crystalize_schema_t* s_schemas;

typedef struct buf_t {
  char* buf;
  uint32_t cur;
  uint32_t capacity;
} buf_t;

typedef struct schema_arr_t {
  crystalize_schema_t** arr;
  uint32_t count;
  uint32_t capacity;
} schema_arr_t;

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

static void buffer_init(buf_t* buf) {
  buf->buf = NULL;
  buf->cur = 0;
  buf->capacity = 0;
}

static void buffer_ensure(buf_t* buf, uint32_t count) {
  uint32_t new_cur = buf->cur + count;
  if (new_cur > buf->capacity) {
    buf->capacity = (new_cur + 1023) & ~1023;
    buf->buf = (char*)realloc(buf->buf, buf->capacity);
  }
}

static void buffer_pad(buf_t* buf, uint32_t count) {
  buffer_ensure(buf, count);
  for (uint32_t index = 0; index < count; ++index) {
    buf->buf[buf->cur + index] = 0;
  }
  buf->cur += count;
}

static void buffer_align(buf_t* buf, uint32_t alignment) {
  uint32_t new_cur = (buf->cur + (alignment - 1)) & ~(alignment - 1);
  if (new_cur > buf->cur) {
    buffer_pad(buf, (new_cur - buf->cur));
  }
}

static void buffer_write_u8(buf_t* buf, uint8_t value) {
  buffer_align(buf, 1);
  buffer_ensure(buf, 1);
  memmove(buf->buf + buf->cur, &value, sizeof(uint8_t));
  buf->cur += 1;
}

static void buffer_write_u32(buf_t* buf, uint32_t value) {
  buffer_align(buf, 4);
  buffer_ensure(buf, 4);
  memmove(buf->buf + buf->cur, &value, sizeof(uint32_t));
  buf->cur += 4;
}

static void buffer_write_u64(buf_t* buf, uint64_t value) {
  buffer_align(buf, 8);
  buffer_ensure(buf, 8);
  memmove(buf->buf + buf->cur, &value, sizeof(uint64_t));
  buf->cur += 8;
}

static void buffer_write(buf_t* buf, const void* data, uint32_t size) {
  buffer_ensure(buf, size);
  memmove(buf->buf + buf->cur, data, size);
  buf->cur += size;
}

static void write_schema(buf_t* buf, const crystalize_schema_t* schema) {
  buffer_write_u32(buf, schema->name_id);
  buffer_write_u32(buf, schema->version);
  buffer_write_u32(buf, schema->field_count);
  if (schema->fields == NULL) {
    buffer_write_u64(buf, 0xffffffffffffffffull);
  }
  else {
    buffer_write_u64(buf, 8);  // offset to the fields data
  }
  for (uint32_t index = 0; index < schema->field_count; ++index) {
    const crystalize_schema_field_t* field = schema->fields + index;
    buffer_write_u32(buf, field->name_id);
    buffer_write_u32(buf, field->count);
    buffer_write_u8(buf, field->type);
  }
}

static uint32_t get_type_alignment(crystalize_type_t type) {
  switch (type) {
    case CRYSTALIZE_BOOL:
      return alignof(bool);
    case CRYSTALIZE_CHAR:
      return alignof(char);
    case CRYSTALIZE_INT8:
      return alignof(int8_t);
    case CRYSTALIZE_INT16:
      return alignof(int16_t);
    case CRYSTALIZE_INT32:
      return alignof(int32_t);
    case CRYSTALIZE_INT64:
      return alignof(int64_t);
    case CRYSTALIZE_UINT8:
      return alignof(uint8_t);
    case CRYSTALIZE_UINT16:
      return alignof(uint16_t);
    case CRYSTALIZE_UINT32:
      return alignof(uint32_t);
    case CRYSTALIZE_UINT64:
      return alignof(uint64_t);
    case CRYSTALIZE_FLOAT:
      return alignof(float);
    case CRYSTALIZE_DOUBLE:
      return alignof(double);
    default:
      crystalize_assert(false, "unknown field type");
      return 0;
  }
}

static uint32_t get_type_size(crystalize_type_t type) {
  switch (type) {
    case CRYSTALIZE_BOOL:
      return sizeof(bool);
    case CRYSTALIZE_CHAR:
      return sizeof(char);
    case CRYSTALIZE_INT8:
      return sizeof(int8_t);
    case CRYSTALIZE_INT16:
      return sizeof(int16_t);
    case CRYSTALIZE_INT32:
      return sizeof(int32_t);
    case CRYSTALIZE_INT64:
      return sizeof(int64_t);
    case CRYSTALIZE_UINT8:
      return sizeof(uint8_t);
    case CRYSTALIZE_UINT16:
      return sizeof(uint16_t);
    case CRYSTALIZE_UINT32:
      return sizeof(uint32_t);
    case CRYSTALIZE_UINT64:
      return sizeof(uint64_t);
    case CRYSTALIZE_FLOAT:
      return sizeof(float);
    case CRYSTALIZE_DOUBLE:
      return sizeof(double);
    default:
      crystalize_assert(false, "unknown field type");
      return 0;
  }
}

static uint32_t get_struct_alignment(const crystalize_schema_t* schema) {
  uint32_t alignment = 0;
  for (int index = 0; index < schema->field_count; ++index) {
    uint32_t field_alignment = get_type_alignment(schema->fields[index].type);
    if (field_alignment > alignment) {
      alignment = field_alignment;
    }
  }
  return alignment;
}

static void write_struct(buf_t* buf, const crystalize_schema_t* schema, const void* data_in) {
  const char* data = (const char*)data_in;

  // pad out to the struct alignment
  const uint32_t struct_alignment = get_struct_alignment(schema);
  buffer_align(buf, struct_alignment);

  for (int index = 0; index < schema->field_count; ++index) {
    const crystalize_schema_field_t* field = schema->fields + index;
    const crystalize_type_t field_type = (crystalize_type_t)field->type;
    const uint32_t alignment = get_type_alignment(field_type);
    const uint32_t field_size = get_type_size(field_type);

    for (int arr_index = 0; arr_index < field->count; ++arr_index) {
      // align for the type
      data = ALIGN_PTR(const char, data, alignment);
      buffer_align(buf, alignment);

      // copy in the field value
      buffer_write(buf, data, field_size);
      data += field_size;
    }
  }
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
  crystalize_assert(schema->field_count > 0, "schema cannot represent an empty struct");

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

static void gather_schemas(schema_arr_t* schemas, uint32_t schema_name_id) {
  // lookup the schema
  const int schema_index = schema_find(schema_name_id);
  crystalize_assert(schema_index != -1, "schema not found");
  crystalize_schema_t* schema = s_schemas + schema_index;

  // check if the schema's already in the list
  for (uint32_t index = 0; index < schemas->count; ++index) {
    if (schema == schemas->arr[index]) {
      // already in the list, bail
      return;
    }
  }

  // ensure there's enough room to add another schema
  if (schemas->count >= schemas->capacity) {
    schemas->capacity += 128;
    schemas->arr = (crystalize_schema_t**)realloc(schemas->arr, schemas->capacity * sizeof(crystalize_schema_t**));
  }

  // add the schema to the list
  schemas->arr[schemas->count] = schema;
  ++schemas->count;
}

static int ptr_compare(const void* a, const void* b) {
  if (a < b) {
    return -1;
  }
  else if (a > b) {
    return 1;
  }
  else {
    return 0;
  }
}

void crystalize_encode(uint32_t schema_name_id, const void* data, char** buf, uint32_t* buf_size) {
  const int schema_index = schema_find(schema_name_id);
  crystalize_assert(schema_index != -1, "schema not found");

  // count up all the unique schemas
  schema_arr_t schemas = { 0 };
  gather_schemas(&schemas, schema_name_id);
  qsort(schemas.arr, schemas.count, sizeof(crystalize_schema_t*), &ptr_compare);

  buf_t buffer;
  buffer_init(&buffer);
  buffer_write_u8(&buffer, 0x63);
  buffer_write_u8(&buffer, 0x72);
  buffer_write_u8(&buffer, 0x79);
  buffer_write_u8(&buffer, 0x73);
  buffer_write_u32(&buffer, 1);
  buffer_write_u32(&buffer, 1);
  buffer_write_u32(&buffer, schemas.count);
  for (uint32_t index = 0; index < schemas.count; ++index) {
    write_schema(&buffer, s_schemas + schema_index);
  }

  write_struct(&buffer, schemas.arr[0], data);

  // free the schemas gather buffer
  free(schemas.arr);
  schemas.arr = NULL;

  *buf = buffer.buf;
  *buf_size = buffer.cur;
}
