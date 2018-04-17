#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "crystalize.h"
#include "encoder.h"
#include "writer.h"

#define ALIGN(x, align) (((x) + (align)-1) & (~((align)-1)))
#define ALIGN_PTR(T, p, align) ((T*)ALIGN((uintptr_t)(p), (uintptr_t)align))

extern crystalize_schema_t s_schema_schema;

typedef struct schema_list_t {
  crystalize_schema_t* entries;
  int count;
  int capacity;
} schema_list_t;

typedef struct pointer_fixup_list_t {
  uint32_t* entries; // list of offsets into the buffer of pointers that need to be fixed
  int count;
  int capacity;
} pointer_fixup_list_t;

typedef struct pointer_remap_t {
  const void* from; // the pointer address in source space
  uint32_t to;      // the offset into the buffer where the pointer should map
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
  writer_t writer;
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

static uint32_t type_get_alignment(crystalize_type_t type) {
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

static uint32_t type_get_size(crystalize_type_t type) {
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

static bool field_is_scalar(const crystalize_schema_field_t* field) {
  return field->type != CRYSTALIZE_STRUCT;
}
static bool field_is_pointer(const crystalize_schema_field_t* field) {
  return field->count == 0;
}
static bool field_is_pointer_counted(const crystalize_schema_field_t* field) {
  return field->count == 0 && field->count_field_name_id != 0;
}

static uint32_t field_get_alignment(const crystalize_schema_field_t* field) {
  if (field_is_pointer(field)) {
    return alignof(void*);
  }
  else {
    return type_get_alignment(field->type);
  }
}

static uint32_t field_get_size(const crystalize_schema_field_t* field) {
  if (field_is_pointer(field)) {
    return sizeof(void*);
  }
  else {
    return type_get_size(field->type);
  }
}

static uint32_t field_get_value_as_uint32(const crystalize_schema_field_t* field, const void* data) {
  switch (field->type) {
    case CRYSTALIZE_INT8:
      return (uint32_t)(*(const int8_t*)data);
    case CRYSTALIZE_INT16:
      return (uint32_t)(*(const int16_t*)data);
    case CRYSTALIZE_INT32:
      return (uint32_t)(*(const int32_t*)data);
    case CRYSTALIZE_UINT8:
      return (uint32_t)(*(const uint8_t*)data);
    case CRYSTALIZE_UINT16:
      return (uint32_t)(*(const uint16_t*)data);
    case CRYSTALIZE_UINT32:
      return (uint32_t)(*(const uint32_t*)data);
    default:
      crystalize_assert(false, "unsupported field type for the count of a counted pointer");
      return 0;
  }
}

static uint32_t struct_get_alignment(const crystalize_schema_t* schema) {
  return schema->alignment;
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

static void gather_schemas_impl(crystalize_encode_result_t* result, schema_list_t* schemas, const crystalize_schema_t* schema) {
  // check if the schema is already in the list
  for (int index = 0; index < schemas->count; ++index) {
    if (schema->name_id == schemas->entries[index].name_id) {
      // bail. already in the list
      return;
    }
  }

  array_grow_if_needed(&schemas->entries, &schemas->count, &schemas->capacity, sizeof(crystalize_schema_t), 128);

  // add the schema to the list
  schemas->entries[schemas->count] = *schema;
  ++schemas->count;

  // recurse on fields
  for (uint32_t index = 0; index < schema->field_count; ++index) {
    const crystalize_schema_field_t* field = schema->fields + index;
    if (field->type == CRYSTALIZE_STRUCT) {
      const crystalize_schema_t* field_schema = crystalize_schema_get(field->struct_name_id, field->struct_version);
      if (field_schema == NULL) {
        int err_msg_len = snprintf(
            NULL,
            0,
            "field has unknown schema. schema_name_id=%08x field_name_id=%08x field_struct_schema_name_id=%08x field_struct_schema_version=%08x",
            schema->name_id,
            field->name_id,
            field->struct_name_id,
            field->struct_version);
        char* err_msg = (char*)crystalize_alloc(err_msg_len + 1);
        snprintf(err_msg,
                 err_msg_len + 1,
                 "field has unknown schema. schema_name_id=%08x field_name_id=%08x field_struct_schema_name_id=%08x field_struct_schema_version=%08x",
                 schema->name_id,
                 field->name_id,
                 field->struct_name_id,
                 field->struct_version);
        result->error = CRYSTALIZE_ERROR_SCHEMA_NOT_FOUND;
        result->error_message = err_msg;
        return;
      }
      gather_schemas_impl(result, schemas, field_schema);
    }
  }
}

static void gather_schemas(crystalize_encode_result_t* result, schema_list_t* schemas, const crystalize_schema_t* schema) {
  gather_schemas_impl(result, schemas, schema);
  if (result->error != CRYSTALIZE_ERROR_NONE) {
    qsort(schemas->entries, schemas->count, sizeof(crystalize_schema_t), &schema_compare);
  }
}

static void pointer_fixup_add(encoder_t* encoder, uint32_t pos) {
  pointer_fixup_list_t* fixups = &encoder->pointer_fixups;
  array_grow_if_needed(&fixups->entries, &fixups->count, &fixups->capacity, sizeof(void**), 128);
  fixups->entries[fixups->count] = pos;
  ++fixups->count;
}

static void pointer_remap_add(encoder_t* encoder, const void* from, uint32_t pos) {
  pointer_remap_list_t* remaps = &encoder->pointer_remaps;
  array_grow_if_needed(&remaps->entries, &remaps->count, &remaps->capacity, sizeof(pointer_remap_t), 128);
  pointer_remap_t* entry = remaps->entries + remaps->count;
  entry->from = from;
  entry->to = pos;
  ++remaps->count;
}

static void convert_pointers_to_offsets(encoder_t* encoder) {
  writer_t* writer = &encoder->writer;
  pointer_fixup_list_t* fixups = &encoder->pointer_fixups;
  pointer_remap_list_t* remaps = &encoder->pointer_remaps;
  for (int fixup_index = 0; fixup_index < fixups->count; ++fixup_index) {
    const uint32_t offset_of_fixup = fixups->entries[fixup_index];
    const void* dest_orig = *(const void**)(writer->buf + offset_of_fixup);
    uint32_t dest = 0;
    for (int remap_index = 0; remap_index < remaps->count; ++remap_index) {
      const pointer_remap_t* remap = remaps->entries + remap_index;
      if (remap->from == dest_orig) {
        dest = remap->to;
        break;
      }
    }
    crystalize_assert(dest != 0, "failed find remap target for fixup pointer");

    // apply the remap as an offset
    intptr_t offset = (int32_t)dest - (int32_t)offset_of_fixup;
    *(int64_t*)(writer->buf + offset_of_fixup) = offset;

    writer_write_u32(&encoder->writer, offset_of_fixup);
  }
}

static void encoder_free(encoder_t* encoder) {
  array_free(&encoder->pointer_remaps.entries, &encoder->pointer_remaps.count, &encoder->pointer_remaps.capacity);
  array_free(&encoder->pointer_fixups.entries, &encoder->pointer_fixups.count, &encoder->pointer_fixups.capacity);
}

static void write_scalars(encoder_t* encoder, crystalize_type_t type, uint32_t count, const void* data_in) {
  writer_t* writer = &encoder->writer;
  const uint32_t alignment = type_get_alignment(type);
  const uint32_t size = type_get_size(type);
  const char* data = (const char*)data_in;

  // align the buffer and the input data
  writer_align(writer, alignment);
  data = ALIGN_PTR(const char, data, alignment);

  // copy everything as a single block
  const uint32_t total_bytes = count * size;
  writer_write(writer, data, total_bytes);
  data += total_bytes;
}

static const char* write_struct(encoder_t* encoder, const crystalize_schema_t* schema, const char* data) {
  writer_t* writer = &encoder->writer;
  const uint32_t struct_alignment = struct_get_alignment(schema);

  // align the start of the struct
  writer_align(writer, struct_alignment);
  data = ALIGN_PTR(const char, data, struct_alignment);
  const char* data_start = data;

  // write out each field
  for (uint32_t field_index = 0; field_index < schema->field_count; ++field_index) {
    const crystalize_schema_field_t* field = schema->fields + field_index;

    // // align to the field's desired alignment
    // const uint32_t field_alignment = field_get_alignment(field);
    // writer_align(writer, field_alignment);
    // data = ALIGN_PTR(const char, data, field_alignment);

    const bool is_scalar = field_is_scalar(field);
    const bool is_pointer = field_is_pointer(field);
    const bool is_pointer_counted = field_is_pointer_counted(field);
    if (is_pointer) {
      // align
      writer_align(writer, alignof(void*));
      data = ALIGN_PTR(const char, data, alignof(void*));

      const void* ptr_value = *(const char**)data;
      if (ptr_value == NULL) {
        // NULL pointer. yay. just write out zeros and move on
        writer_pad(writer, sizeof(void*));
        data += sizeof(void*);
      }
      else {
        // write out a pointer to be fixed up later
        pointer_fixup_add(encoder, writer->cur);
        writer_write(writer, &ptr_value, sizeof(void*));
        data += sizeof(void*);

        uint32_t target_count = 1;
        if (is_pointer_counted) {
          // lookup the field containing the count
          const crystalize_schema_field_t* count_field = NULL;
          const char* data_field = data_start;
          for (uint32_t index = 0; index < schema->field_count; ++index) {
            data_field = ALIGN_PTR(const char, data_field, field_get_alignment(schema->fields + index));
            if (schema->fields[index].name_id == field->count_field_name_id) {
              count_field = schema->fields + index;
              break;
            }
            data_field += field_get_size(schema->fields + index);
          }
          crystalize_assert(count_field != NULL,
                            "internal error: failed to find the referenced count field for a counted pointer");

          // extract the count
          target_count = field_get_value_as_uint32(count_field, data_field);
        }
        write_queue_push(
            &encoder->todo_list, field->type, crystalize_schema_get(field->struct_name_id, field->struct_version), target_count, ptr_value);
      }
    }
    else {
      // simple
      if (is_scalar) {
        // align to the scalar type
        uint32_t field_alignment = type_get_alignment(field->type);
        writer_align(writer, field_alignment);
        data = ALIGN_PTR(const char, data, field_alignment);

        // simple scalar type, just copy it in
        uint32_t byte_size = field->count * type_get_size(field->type);
        writer_write(writer, data, byte_size);
        data += byte_size;
      }
      else {
        // recurse here
        const crystalize_schema_t* field_schema = crystalize_schema_get(field->struct_name_id, field->struct_version);
        crystalize_assert(field_schema != NULL, "failed to find field schema");

        // NOTE: write_struct() will do the alignment
        data = write_struct(encoder, field_schema, data);
      }
    }
  }

  // pad out to the struct's alignment
  writer_align(writer, struct_alignment);
  data = ALIGN_PTR(const char, data, struct_alignment);

  return data;
}

static void encoder_run(encoder_t* encoder) {
  write_queue_t* todo_list = &encoder->todo_list;
  while (todo_list->count > 0) {
    write_queue_entry_t* todo = write_queue_first(todo_list);
    const char* data = todo->data;

    pointer_remap_add(encoder, data, encoder->writer.cur);

    if (todo->type == CRYSTALIZE_STRUCT) {
      for (uint32_t index = 0; index < todo->count; ++index) {
        data = write_struct(encoder, todo->schema, data);
      }
    }
    else {
      write_scalars(encoder, todo->type, todo->count, data);
    }

    write_queue_shift(todo_list);
  }
}

void encoder_encode(const crystalize_schema_t* schema, const void* data, crystalize_encode_result_t* result) {
  encoder_t encoder = {0};

  // gather up and count up all the unique schemas
  schema_list_t schemas = {0};
  gather_schemas(result, &schemas, schema);
  if (result->error != CRYSTALIZE_ERROR_NONE) {
    return;
  }

  // file header
  writer_write_u8(&encoder.writer, 0x63);
  writer_write_u8(&encoder.writer, 0x72);
  writer_write_u8(&encoder.writer, 0x79);
  writer_write_u8(&encoder.writer, 0x73);
  writer_write_u32(&encoder.writer, CRYSTALIZE_FILE_VERSION);
  writer_write_u32(&encoder.writer, 1); // endian
  writer_write_u8(&encoder.writer, (uint8_t)sizeof(void*));
  writer_align(&encoder.writer, 4);
  const uint32_t header_data_start_offset = encoder.writer.cur;
  writer_write_u32(&encoder.writer, 0); // offset to the start of the data buffer
  const uint32_t pointer_table_start_offset = encoder.writer.cur;
  writer_write_u32(&encoder.writer, 0); // offset to the start of the pointer fixup pointer_table
  const uint32_t pointer_table_count_offset = encoder.writer.cur;
  writer_write_u32(&encoder.writer, 0); // number of pointers in the pointer table
  writer_write_u32(&encoder.writer, schemas.count);

  // schemas
  write_queue_push(&encoder.todo_list, CRYSTALIZE_STRUCT, &s_schema_schema, schemas.count, schemas.entries);
  encoder_run(&encoder);

  // write into the header the offset to the start of the data
  writer_align(&encoder.writer, schema->alignment);
  memmove(encoder.writer.buf + header_data_start_offset, &encoder.writer.cur, sizeof(uint32_t));

  // data
  write_queue_push(&encoder.todo_list, CRYSTALIZE_STRUCT, schema, 1, data);
  encoder_run(&encoder);

  // fixup the pointers
  writer_align(&encoder.writer, alignof(uint32_t));
  memmove(encoder.writer.buf + pointer_table_start_offset, &encoder.writer.cur, sizeof(uint32_t));
  memmove(encoder.writer.buf + pointer_table_count_offset, &encoder.pointer_fixups.count, sizeof(uint32_t));
  convert_pointers_to_offsets(&encoder);

  result->buf = encoder.writer.buf;
  result->buf_size = encoder.writer.cur;

  // free the schemas gather buffer and the encoder
  array_free(&schemas.entries, &schemas.count, &schemas.capacity);
  encoder_free(&encoder);
}
