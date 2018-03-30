#include "encode.h"
#include "config.h"
#include "crystalize.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN(x, align) (((x) + (align)-1) & (~((align)-1)))
#define ALIGN_PTR(T, p, align) ((T*)ALIGN((uintptr_t)(p), (uintptr_t)align))

extern crystalize_schema_t s_schema_schema;

static const uint32_t FILE_VERSION = 0u;

typedef struct schema_list_t {
  const crystalize_schema_t** entries;
  int count;
  int capacity;
} schema_list_t;

typedef struct buf_t {
  char* buf;
  uint32_t cur;
  uint32_t capacity;
} buf_t;

typedef struct pointer_fixup_t {
  void* from;     // pointer to the pointer that needs to be fixed
  const void* to; // the destination of the pointer (in source space)
} pointer_fixup_t;

typedef struct pointer_fixup_list_t {
  pointer_fixup_t* entries;
  int count;
  int capacity;
} pointer_fixup_list_t;

typedef struct pointer_remap_t {
  const void* from; // the pointer address in source space
  const void* to;   // the pointer address remapped in destination space
} pointer_remap_t;

typedef struct pointer_remap_list_t {
  pointer_remap_t* entries;
  int count;
  int capacity;
} pointer_remap_list_t;

typedef struct write_queue_entry_t {
  const crystalize_schema_t* schema;
  const void* data;
  uint32_t count;
  crystalize_type_t type;
} write_queue_entry_t;

typedef struct write_queue_t {
  write_queue_entry_t* entries;
  int count;
  int capacity;
} write_queue_t;

typedef struct encoder_t {
  buf_t buf;
  write_queue_t todo_list;
  pointer_fixup_list_t pointer_fixups;
  pointer_remap_list_t pointer_remaps;
} encoder_t;

static void array_free(void* entries_ptr, int* count_ptr, int* capacity_ptr) {
  crystalize_free(*(void**)entries_ptr);
  *(void**)entries_ptr = NULL;
  *count_ptr = 0;
  *capacity_ptr = 0;
}

static void array_grow_if_needed(void* entries_ptr, int* count_ptr, int* capacity_ptr, int element_size, int growth_step) {
  void* entries = *(void**)entries_ptr;
  int count = *count_ptr;
  int capacity = *capacity_ptr;
  if (count >= capacity) {
    capacity += growth_step;

    entries = crystalize_realloc(entries, capacity * element_size);
    *capacity_ptr = capacity;
    *(void**)entries_ptr = entries;
  }
}

// static char* buf_pos(buf_t* buf) {
//   return buf->buf + buf->cur;
// }

static void buf_ensure(buf_t* buf, uint32_t count) {
  uint32_t new_cur = buf->cur + count;
  if (new_cur > buf->capacity) {
    buf->capacity = (new_cur + 1023) & ~1023;
    buf->buf = (char*)crystalize_realloc(buf->buf, buf->capacity);
  }
}

static void buf_pad(buf_t* buf, uint32_t count) {
  buf_ensure(buf, count);
  for (uint32_t index = 0; index < count; ++index) {
    buf->buf[buf->cur + index] = 0;
  }
  buf->cur += count;
}

static void buf_align(buf_t* buf, uint32_t alignment) {
  uint32_t new_cur = (buf->cur + (alignment - 1)) & ~(alignment - 1);
  if (new_cur > buf->cur) {
    buf_pad(buf, (new_cur - buf->cur));
  }
}

static void buf_write(buf_t* buf, const void* data, uint32_t size) {
  buf_ensure(buf, size);
  memmove(buf->buf + buf->cur, data, size);
  buf->cur += size;
}

static void buf_write_u8(buf_t* buf, uint8_t value) {
  buf_align(buf, 1);
  buf_write(buf, &value, 1);
}

static void buf_write_u32(buf_t* buf, uint32_t value) {
  buf_align(buf, 4);
  buf_write(buf, &value, 4);
}

static void write_queue_push(write_queue_t* queue, crystalize_type_t type, const crystalize_schema_t* schema, uint32_t count, const void* data) {
  array_grow_if_needed(&queue->entries, &queue->count, &queue->capacity, sizeof(write_queue_entry_t), 128);
  write_queue_entry_t* entry = queue->entries + queue->count;
  entry->type = type;
  entry->schema = schema;
  entry->data = data;
  entry->count = count;
  ++queue->count;
}

static write_queue_entry_t* write_queue_first(write_queue_t* queue) {
  return queue->entries;
}

static void write_queue_shift(write_queue_t* queue) {
  --queue->count;
  if (queue->count > 0) {
    memcpy(queue->entries, queue->entries + 1, queue->count * sizeof(write_queue_entry_t));
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
  return 4;
}

static int schema_compare(const void* a, const void* b) {
  const crystalize_schema_t* schema_a = (const crystalize_schema_t*)a;
  const crystalize_schema_t* schema_b = (const crystalize_schema_t*)b;
  if (schema_a->name_id < schema_b->name_id) {
    return -1;
  }
  else if (schema_a->name_id > schema_b->name_id) {
    return 1;
  }
  else {
    return 0;
  }
}

static void gather_schemas_impl(schema_list_t* schemas, const crystalize_schema_t* schema) {
  // check if the schema is already in the list
  for (int index = 0; index < schemas->count; ++index) {
    if (schema->name_id == schemas->entries[index]->name_id) {
      // bail. already in the list
      return;
    }
  }

  array_grow_if_needed(&schemas->entries, &schemas->count, &schemas->capacity, sizeof(crystalize_schema_t*), 128);

  // add the schema to the list
  schemas->entries[schemas->count] = schema;
  ++schemas->count;

  // recurse on fields
  for (uint32_t index = 0; index < schema->field_count; ++index) {
    const crystalize_schema_field_t* field = schema->fields + index;
    if (field->type == CRYSTALIZE_STRUCT) {
      const crystalize_schema_t* field_schema = crystalize_schema_get(field->complex_type_id);
      crystalize_assert(field_schema != NULL, "field has unknown schema");
      gather_schemas_impl(schemas, field_schema);
    }
  }
}

static void gather_schemas(schema_list_t* schemas, const crystalize_schema_t* schema) {
  gather_schemas_impl(schemas, schema);
  qsort(schemas->entries, schemas->count, sizeof(crystalize_schema_t*), &schema_compare);
}

// static void pointer_fixup_add(encoder_t* encoder, void* from, const void* to) {
//   pointer_fixup_list_t* fixups = &encoder->pointer_fixups;
//   array_grow_if_needed(&fixups->entries, &fixups->count, &fixups->capacity, sizeof(pointer_fixup_t), 128);
//   pointer_fixup_t* entry = fixups->entries + fixups->count;
//   entry->from = from;
//   entry->to = to;
//   ++fixups->count;
// }

// static void pointer_remap_add(encoder_t* encoder, const void* from, const void* to) {
//   pointer_remap_list_t* remaps = &encoder->pointer_remaps;
//   array_grow_if_needed(&remaps->entries, &remaps->count, &remaps->capacity, sizeof(pointer_remap_t), 128);
//   pointer_remap_t* entry = remaps->entries + remaps->count;
//   entry->from = from;
//   entry->to = to;
//   ++remaps->count;
// }

// static void fixup_pointers(encoder_t* encoder) {
//   pointer_fixup_list_t* fixups = &encoder->pointer_fixups;
//   pointer_remap_list_t* remaps = &encoder->pointer_remaps;
//   for (int fixup_index = 0; fixup_index < fixups->count; ++fixup_index) {
//     const pointer_fixup_t* fixup = fixups->entries + fixup_index;
//     void** addr = fixup->from;
//     const void* dest_orig = fixup->to;
//     const void* dest = NULL;
//     for (int remap_index = 0; remap_index < remaps->count; ++remap_index) {
//       const pointer_remap_t* remap = remaps->entries + remap_index;
//       if (remap->from == dest_orig) {
//         dest = remap->to;
//         break;
//       }
//     }
//     crystalize_assert(dest != NULL, "failed find remap target for fixup pointer");
//
//     // apply the remap as an offset
//     intptr_t offset = (intptr_t)dest - (intptr_t)addr;
//     *addr = (void*)offset;
//   }
// }

static void encoder_free(encoder_t* encoder) {
  array_free(&encoder->pointer_remaps.entries, &encoder->pointer_remaps.count, &encoder->pointer_remaps.capacity);
  array_free(&encoder->pointer_fixups.entries, &encoder->pointer_fixups.count, &encoder->pointer_fixups.capacity);
}

static void write_scalars(encoder_t* encoder, crystalize_type_t type, uint32_t count, const void* data_in) {
  buf_t* buf = &encoder->buf;
  const uint32_t alignment = get_type_alignment(type);
  const uint32_t size = get_type_size(type);
  const char* data = (const char*)data_in;

  buf_align(buf, alignment);
  ALIGN_PTR(const char, data, alignment);
  for (uint32_t index = 0; index < count; ++index) {
    buf_write(buf, data, size);
    data += size;
  }
}

static void write_structs(encoder_t* encoder, const crystalize_schema_t* schema, uint32_t count, const void* data_in) {
  buf_t* buf = &encoder->buf;
  const uint32_t alignment = get_struct_alignment(schema);
  const char* data = (const char*)data_in;

  buf_align(buf, alignment);
  ALIGN_PTR(const char, data, alignment);
  for (uint32_t arr_index = 0; arr_index < count; ++arr_index) {
    for (uint32_t field_index = 0; field_index < schema->field_count; ++field_index) {
      const crystalize_schema_field_t* field = schema->fields + field_index;
      if (field->type == CRYSTALIZE_STRUCT) {
        buf_write_u32(buf, 0xbbbbbbbbu);
        data += 8;
      }
      else {
        const uint32_t field_alignment = get_type_alignment(field->type);
        const uint32_t field_size = get_type_size(field->type);

        // align for the type
        buf_align(buf, field_alignment);
        data = ALIGN_PTR(const char, data, field_alignment);

        // copy the field value
        buf_write(buf, data, field_size);
        data += field_size;
      }
    }
  }
}

static void write_typed_data(encoder_t* encoder, crystalize_type_t type, const crystalize_schema_t* schema, uint32_t arr_count, const void* data) {
  if (type == CRYSTALIZE_STRUCT) {
    write_structs(encoder, schema, arr_count, data);
  }
  else {
    write_scalars(encoder, type, arr_count, data);
  }

  // // pad out to proper alignment
  // const uint32_t alignment = get_alignment(type, is_pointer, schema);
  // buf_align(&encoder->buf, alignment);
  //
  // // register this data as pointer target
  // pointer_remap_add(encoder, data, buf_pos(&encoder->buf));
  //
  // for (uint32_t arr_index = 0; arr_index < arr_count; ++arr_index) {
  //   for (uint32_t field_index = 0; field_index < schema->field_count; ++field_index) {
  //
  //   }
  //
  //   // pad out to the structure size
  //   buf_align(&encoder->buf, alignment);
  // }
}

static void encoder_run(encoder_t* encoder) {
  write_queue_t* todo_list = &encoder->todo_list;
  while (todo_list->count > 0) {
    write_queue_entry_t* todo = write_queue_first(todo_list);
    write_typed_data(encoder, todo->type, todo->schema, todo->count, todo->data);
    write_queue_shift(todo_list);
  }
}

void encoder_encode(const crystalize_schema_t* schema, const void* data, char** buf, uint32_t* buf_size) {
  encoder_t encoder = {{0}};

  // gather up and count up all the unique schemas
  schema_list_t schemas = {0};
  gather_schemas(&schemas, schema);

  // file header
  buf_write_u8(&encoder.buf, 0x63);
  buf_write_u8(&encoder.buf, 0x72);
  buf_write_u8(&encoder.buf, 0x79);
  buf_write_u8(&encoder.buf, 0x73);
  buf_write_u32(&encoder.buf, FILE_VERSION);
  buf_write_u32(&encoder.buf, 1);
  buf_write_u32(&encoder.buf, schemas.count);

  // schemas
  write_queue_push(&encoder.todo_list, CRYSTALIZE_STRUCT, &s_schema_schema, schemas.count, schemas.entries);
  encoder_run(&encoder);

  // data
  // write_queue_push(&encoder.todo_list, CRYSTALIZE_STRUCT, schema, 1, data);
  // encoder_run(&encoder);

  // data
  // write_queue_push(&todo_list, CRYSTALIZE_POINTER, schema, data, 1);
  // while (todo_list.count > 0) {
  //   write_queue_entry_t* todo = write_queue_top(&todo_list);
  //   for (uint32_t index = 0; index < todo->count; ++index) {
  //     write_with_schema(&buffer, &pointer_table, &todo_list, todo->schema, todo->data);
  //   }
  //   write_queue_pop(&todo_list);
  // }

  // pointer_table_fixup_to_offsets(&pointer_table);
  // pointer_table_free(&pointer_table);

  *buf = encoder.buf.buf;
  *buf_size = encoder.buf.cur;

  // free the schemas gather buffer
  array_free(&schemas.entries, &schemas.count, &schemas.capacity);
  encoder_free(&encoder);
}
