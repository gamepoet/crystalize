#include "crystalize.h"
#include "config.h"
#include "encode.h"
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

// #define ALIGN(x, align) (((x) + (align)-1) & (~((align)-1)))
// #define ALIGN_PTR(T, p, align) ((T*)ALIGN((uintptr_t)(p), (uintptr_t)align))

// #define PACKED_SET_POINTER_FLAG(packed, val) (((packed) & 0xfffffffe) | ((val) & 0x1))
// #define PACKED_GET_POINTER_FLAG(packed, val) ((()))
// #define PACKED_SET_TYPE(packed, val) (((packed) >> 1) & 0xfu)
//
// #define FIELD_TYPE(field) ((field)->type_count_packed & 0xfu)
// #define FIELD_COUNT(field) (((field)->type_count_packed & 0xfffffff0u) >> 4)
// #define FIELD_SET_TYPE_COUNT(field, type, count) ((field)->type_count_packed = (((count) << 4) | ((type)&0xfu)))

static int s_schemas_capacity;
static int s_schemas_count;
static crystalize_schema_t* s_schemas;

crystalize_schema_t s_schema_schema;       // the schema for crystalize_schema_t
static crystalize_schema_t s_schema_schema_field; // the schema for crystalize_schema_field_t
static crystalize_schema_field_t s_schema_schema_fields[4];
static crystalize_schema_field_t s_schema_schema_field_fields[5];

// typedef struct buf_t {
//   char* buf;
//   uint32_t cur;
//   uint32_t capacity;
// } buf_t;
//
// typedef struct pointer_fixup_t {
//   void* addr;
//   const void* target;
// } pointer_fixup_t;
//
// typedef struct pointer_remap_t {
//   const void* addr;
//   const void* target;
// } pointer_remap_t;
//
// typedef struct pointer_table_t {
//   pointer_fixup_t* fixups;
//   pointer_remap_t* remaps;
//   int fixups_count;
//   int fixups_capacity;
//   int remaps_count;
//   int remaps_capacity;
// } pointer_table_t;
//
// typedef struct write_queue_entry_t {
//   const crystalize_schema_t* schema;
//   const void* data;
//   uint32_t count;
//   crystalize_type_t type;
// } write_queue_entry_t;
//
// typedef struct write_queue_t {
//   write_queue_entry_t* entries;
//   int count;
//   int capacity;
// } write_queue_t;

// void write_queue_push(
//     write_queue_t* queue, crystalize_type_t type, const crystalize_schema_t* schema, const void* data, uint32_t count) {
//   if (queue->count >= queue->capacity) {
//     queue->capacity += 128;
//     queue->entries =
//         (write_queue_entry_t*)crystalize_realloc(queue->entries, queue->capacity * sizeof(write_queue_entry_t));
//   }
//   write_queue_entry_t* entry = queue->entries + queue->count;
//   entry->type = type;
//   entry->schema = schema;
//   entry->data = data;
//   entry->count = count;
//   ++queue->count;
// }
//
// write_queue_entry_t* write_queue_top(write_queue_t* queue) {
//   return queue->entries + (queue->count - 1);
// }
//
// void write_queue_pop(write_queue_t* queue) {
//   --queue->count;
// }

// static void pointer_table_fixup_to_offsets(const pointer_table_t* table) {
// }
//
// static void pointer_table_free(pointer_table_t* table) {
//   crystalize_free(table->fixups);
//   crystalize_free(table->remaps);
//   table->fixups = NULL;
//   table->remaps = NULL;
//   table->fixups_count = 0;
//   table->fixups_capacity = 0;
//   table->remaps_count = 0;
//   table->remaps_capacity = 0;
// }

// typedef struct writer_t {
//   buf_t buf;
//   void** pointers;
//
//   uint32_t pointers_count;
//   uint32_t pointers_capacity;
// } writer_t;

// typedef struct reader_t {
//   buf_t buf;
//   bool error;
// } reader_t;

// static void buffer_write_u64(buf_t* buf, uint64_t value) {
//   buffer_align(buf, 8);
//   buffer_ensure(buf, 8);
//   memmove(buf->buf + buf->cur, &value, sizeof(uint64_t));
//   buf->cur += 8;
// }

// static void buffer_write(buf_t* buf, const void* data, uint32_t size) {
//   buffer_ensure(buf, size);
//   memmove(buf->buf + buf->cur, data, size);
//   buf->cur += size;
// }

// static void write_schema(buf_t* buf, const crystalize_schema_t* schema) {
//   buffer_write_u32(buf, schema->name_id);
//   buffer_write_u32(buf, schema->version);
//   buffer_write_u32(buf, schema->field_count);
//   if (schema->fields == NULL) {
//     buffer_write_u64(buf, 0xffffffffffffffffull);
//   }
//   else {
//     buffer_write_u64(buf, 8);  // offset to the fields data
//   }
//   for (uint32_t index = 0; index < schema->field_count; ++index) {
//     const crystalize_schema_field_t* field = schema->fields + index;
//     buffer_write_u32(buf, field->name_id);
//     buffer_write_u32(buf, field->type_count_packed);
//     buffer_write_u32(buf, field->ref_type_name_id);
//   }
// }

// static uint32_t get_type_alignment(crystalize_type_t type) {
//   switch (type) {
//     case CRYSTALIZE_BOOL:
//       return alignof(bool);
//     case CRYSTALIZE_CHAR:
//       return alignof(char);
//     case CRYSTALIZE_INT8:
//       return alignof(int8_t);
//     case CRYSTALIZE_INT16:
//       return alignof(int16_t);
//     case CRYSTALIZE_INT32:
//       return alignof(int32_t);
//     case CRYSTALIZE_INT64:
//       return alignof(int64_t);
//     case CRYSTALIZE_UINT8:
//       return alignof(uint8_t);
//     case CRYSTALIZE_UINT16:
//       return alignof(uint16_t);
//     case CRYSTALIZE_UINT32:
//       return alignof(uint32_t);
//     case CRYSTALIZE_UINT64:
//       return alignof(uint64_t);
//     case CRYSTALIZE_FLOAT:
//       return alignof(float);
//     case CRYSTALIZE_DOUBLE:
//       return alignof(double);
//     case CRYSTALIZE_POINTER:
//       return alignof(void*);
//     default:
//       crystalize_assert(false, "unknown field type");
//       return 0;
//   }
// }
//
// static uint32_t get_type_size(crystalize_type_t type) {
//   switch (type) {
//     case CRYSTALIZE_BOOL:
//       return sizeof(bool);
//     case CRYSTALIZE_CHAR:
//       return sizeof(char);
//     case CRYSTALIZE_INT8:
//       return sizeof(int8_t);
//     case CRYSTALIZE_INT16:
//       return sizeof(int16_t);
//     case CRYSTALIZE_INT32:
//       return sizeof(int32_t);
//     case CRYSTALIZE_INT64:
//       return sizeof(int64_t);
//     case CRYSTALIZE_UINT8:
//       return sizeof(uint8_t);
//     case CRYSTALIZE_UINT16:
//       return sizeof(uint16_t);
//     case CRYSTALIZE_UINT32:
//       return sizeof(uint32_t);
//     case CRYSTALIZE_UINT64:
//       return sizeof(uint64_t);
//     case CRYSTALIZE_FLOAT:
//       return sizeof(float);
//     case CRYSTALIZE_DOUBLE:
//       return sizeof(double);
//     case CRYSTALIZE_POINTER:
//       return sizeof(void*);
//     default:
//       crystalize_assert(false, "unknown field type");
//       return 0;
//   }
// }

// static uint32_t get_struct_alignment(const crystalize_schema_t* schema) {
//   uint32_t alignment = 0;
//   for (int index = 0; index < schema->field_count; ++index) {
//     const crystalize_schema_field_t* field = schema->fields + index;
//     uint32_t field_alignment = get_type_alignment(FIELD_TYPE(field));
//     if (field_alignment > alignment) {
//       alignment = field_alignment;
//     }
//   }
//   return alignment;
// }

// static void write_with_schema(buf_t* buf,
//                               pointer_table_t* pointer_table,
//                               write_queue_t* todo_list,
//                               const crystalize_schema_t* schema,
//                               const void* data_in) {
//   const char* data = (const char*)data_in;
//
//   // pad out to the struct alignment
//   const uint32_t struct_alignment = get_struct_alignment(schema);
//   buffer_align(buf, struct_alignment);
//
//   pointer_table_add_remap(pointer_table, data, buf->buf + buf->cur);
//
//   for (int index = 0; index < schema->field_count; ++index) {
//     const crystalize_schema_field_t* field = schema->fields + index;
//     const crystalize_type_t field_type = (crystalize_type_t)FIELD_TYPE(field);
//     const uint32_t field_count = FIELD_COUNT(field);
//     const uint32_t alignment = get_type_alignment(field_type);
//     const uint32_t field_size = get_type_size(field_type);
//
//     for (uint32_t arr_index = 0; arr_index < field_count; ++arr_index) {
//       // align for the type
//       data = ALIGN_PTR(const char, data, alignment);
//       buffer_align(buf, alignment);
//
//       // copy in the field value
//       if (field_type != CRYSTALIZE_POINTER) {
//         buffer_write(buf, data, field_size);
//       }
//       else {
//         if (data != NULL) {
//           // record the pointer for fixup later
//           pointer_table_add_fixup(pointer_table, buf->buf + buf->cur, data);
//
//           // add the pointee to the todo list
//           const crystalize_schema_t* field_schema = NULL;
//           crystalize_schema_get(field->ref_type_name_id);
//           crystalize_assert(field_schema != NULL, "pointer field references unknown schema type");
//           write_queue_push(todo_list, field_type, field_schema, data, field_count);
//         }
//         buffer_pad(buf, field_size);
//       }
//       data += field_size;
//     }
//   }
// }

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

  crystalize_schema_init(&s_schema_schema_field, "__crystalize_schema_field", 0, s_schema_schema_field_fields, 3);
  crystalize_schema_field_init(s_schema_schema_field_fields + 0, "name_id", CRYSTALIZE_UINT32, 1, 0);
  crystalize_schema_field_init(s_schema_schema_field_fields + 1, "count", CRYSTALIZE_UINT32, 1, 0);
  crystalize_schema_field_init(s_schema_schema_field_fields + 2, "type", CRYSTALIZE_UINT8, 1, 0);

  crystalize_schema_init(&s_schema_schema, "__crystalize_schema", 0, s_schema_schema_fields, 4);
  crystalize_schema_field_init(s_schema_schema_fields + 0, "name_id", CRYSTALIZE_UINT32, 1, 0);
  crystalize_schema_field_init(s_schema_schema_fields + 1, "version", CRYSTALIZE_UINT32, 1, 0);
  crystalize_schema_field_init(s_schema_schema_fields + 2, "field_count", CRYSTALIZE_UINT32, 1, 0);
  crystalize_schema_field_init(
      s_schema_schema_fields + 3, "fields", CRYSTALIZE_STRUCT, 1, s_schema_schema_field.name_id);
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
                                  uint32_t count,
                                  uint32_t ref_type_name_id) {
  crystalize_assert(field != NULL, "field cannot be null");
  field->name_id = fnv1a(name, strlen(name));
  field->type = type;
  field->count = count;
  field->complex_type_id = ref_type_name_id;
  field->ref_field_index = 0;
  field->pointer = false;
}

void crystalize_schema_init(crystalize_schema_t* schema,
                            const char* name,
                            uint32_t version,
                            const crystalize_schema_field_t* fields,
                            uint32_t field_count) {
  crystalize_assert(schema != NULL, "schema cannot be null");
  schema->name_id = fnv1a(name, strlen(name));
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

void crystalize_encode(uint32_t schema_name_id, const void* data, char** buf, uint32_t* buf_size) {
  const int schema_index = schema_find(schema_name_id);
  crystalize_assert(schema_index != -1, "schema not found");
  const crystalize_schema_t* schema = s_schemas + schema_index;

  encoder_encode(schema, data, buf, buf_size);
}

void* crystalize_decode(uint32_t schema_name_id, char* buf, uint32_t buf_size) {
  const int schema_index = schema_find(schema_name_id);
  crystalize_assert(schema_index != -1, "schema not found");

  // buf_t buffer;
  // buffer.buf = buf;
  // buffer.capacity = buf_size;
  // buffer.cur = 0;
  //
  // // ensure the whole file header is available
  // if (buffer.capacity < 20) {
  //   // TODO: report error: invalid header: short read
  //   return NULL;
  // }
  //
  // // read the file header
  // const uint8_t* magic = buffer.buf + 0;
  // const uint32_t file_version = *(uint32_t*)(buffer.buf + 4);
  // const uint32_t endian = *(uint32_t*)(buffer.buf + 8);
  // const uint32_t schema_count = *(uint32_t*)(buffer.buf + 12);
  // const uint32_t field_count = *(uint32_t*)(buffer.buf + 16);
  // buffer.cur = 20;
  // if (magic[0] != 0x63 && magic[1] != 0x72 && magic[2] != 0x79 && magic[3] != 0x73) {
  //   // TODO: report error: invalid header: magic mismatch
  //   return NULL;
  // }
  // if (file_version != FILE_VERSION) {
  //   // TODO: report error: invalid file format version
  //   return NULL;
  // }
  // if (endian != 1u) {
  //   // TODO: report error: endian mismatch
  //   return NULL;
  // }
  //
  // // read the schemas
  // crystalize_schema_t* schemas = (crystalize_schema_t*)crystalize_malloc(schema_count * sizeof(crystalize_schema_t));
  // crystalize_schema_field_t* fields = (crystalize_schema_field_t*)crystalize_malloc(field_count *
  // sizeof(crystalize_schema_field_t)); int schema_cur = 0; int field_cur = 0; for (uint32_t index = 0; index <
  // schema_count; ++index) {
  //   crystalize_schema_t* schema = schemas + schema_cur;
  //   crystalize_schema_field_t* field_beg = fields + field_cur;
  //   read_schema(&buffer, schema, field_beg);
  // }

  return NULL;
}
