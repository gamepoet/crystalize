#include <stdalign.h>
#include <string.h>
#include "crystalize.h"
#include "encoder.h"

typedef struct reader_t {
  char* buf;
  uint32_t cur;
  uint32_t size;
  const char* error;
} reader_t;

typedef struct decoder_t {
  reader_t reader;
} decoder_t;

static char* read_pos(reader_t* reader) {
  return reader->buf + reader->cur;
}

static bool read_ensure(reader_t* reader, uint32_t count) {
  if (reader->error == NULL) {
    if (reader->cur + count > reader->size) {
      reader->error = "unexpected EOF";
      return false;
    }
  }
  return true;
}

static void read_consume(reader_t* reader, uint32_t count) {
  if (read_ensure(reader, count)) {
    reader->cur += count;
  }
}

static void read_align(reader_t* reader, uint32_t alignment) {
  uint32_t new_cur = (reader->cur + (alignment - 1)) & ~(alignment - 1);
  uint32_t offset = new_cur - reader->cur;
  if (read_ensure(reader, offset)) {
    reader->cur = new_cur;
  }
}

static void read_bytes(reader_t* reader, void* dest, uint32_t count) {
  if (read_ensure(reader, count)) {
    memmove(dest, reader->buf + reader->cur, count);
    reader->cur += count;
  }
}

static void read_u8(reader_t* reader, uint8_t* val) {
  read_bytes(reader, val, 1);
}

static void read_u32(reader_t* reader, uint32_t* val) {
  read_align(reader, 4);
  read_bytes(reader, val, 4);
}

void* encoder_decode(const crystalize_schema_t* schema, char* buf, uint32_t buf_size, crystalize_decode_result_t* result) {
  decoder_t decoder = {0};
  decoder.reader.buf = buf;
  decoder.reader.size = buf_size;
  decoder.reader.cur = 0;
  decoder.reader.error = NULL;

  // read the file header
  uint8_t magic[4];
  uint32_t file_version;
  uint32_t endian;
  uint8_t pointer_size;
  uint32_t data_offset;
  uint32_t pointer_table_offset;
  uint32_t pointer_table_count;
  uint32_t schema_count;
  read_u8(&decoder.reader, magic + 0);
  read_u8(&decoder.reader, magic + 1);
  read_u8(&decoder.reader, magic + 2);
  read_u8(&decoder.reader, magic + 3);
  read_u32(&decoder.reader, &file_version);
  read_u32(&decoder.reader, &endian);
  read_u8(&decoder.reader, &pointer_size);
  read_u32(&decoder.reader, &data_offset);
  read_u32(&decoder.reader, &pointer_table_offset);
  read_u32(&decoder.reader, &pointer_table_count);
  read_u32(&decoder.reader, &schema_count);
  if (decoder.reader.error) {
    result->error = CRYSTALIZE_ERROR_UNEXPECTED_EOF;
    return NULL;
  }
  if (magic[0] != 0x63 || magic[1] != 0x72 || magic[2] != 0x79 || magic[3] != 0x73) {
    result->error = CRYSTALIZE_ERROR_FILE_HEADER_MALFORMED;
    return NULL;
  }
  if (file_version != CRYSTALIZE_FILE_VERSION) {
    result->error = CRYSTALIZE_ERROR_FILE_VERSION_MISMATCH;
    return NULL;
  }
  if (endian != 0x01u) {
    result->error = CRYSTALIZE_ERROR_ENDIAN_MISMATCH;
    return NULL;
  }
  if (pointer_size != sizeof(void*)) {
    result->error = CRYSTALIZE_ERROR_POINTER_SIZE_MISMATCH;
    return NULL;
  }
  if (data_offset >= buf_size) {
    // offset to data start is invalid
    result->error = CRYSTALIZE_ERROR_DATA_OFFSET_IS_INVALID;
    return NULL;
  }
  if (pointer_table_offset >= buf_size) {
    // offset to the pointer tabel is invalid
    result->error = CRYSTALIZE_ERROR_POINTER_TABLE_OFFSET_IS_INVALID;
    return NULL;
  }
  if (pointer_table_offset + (pointer_table_count * sizeof(uint32_t)) > buf_size) {
    result->error = CRYSTALIZE_ERROR_UNEXPECTED_EOF;
    return NULL;
  }

  // load the schema table
  read_align(&decoder.reader, alignof(crystalize_schema_t));
  const crystalize_schema_t* schemas = (const crystalize_schema_t*)read_pos(&decoder.reader);
  read_consume(&decoder.reader, schema_count * sizeof(crystalize_schema_t));
  if (decoder.reader.error) {
    result->error = CRYSTALIZE_ERROR_UNEXPECTED_EOF;
    return NULL;
  }

  // TODO: check the pointers in the schema table

  // TODO: handle mismatched schemas
  schemas = NULL;

  // get pointer to the root of the data graph
  void* data = decoder.reader.buf + data_offset;

  // fixup pointers
  const uint32_t* pointer_table = (const uint32_t*)(decoder.reader.buf + pointer_table_offset);
  if ((const char*)pointer_table >= decoder.reader.buf + decoder.reader.size) {
    result->error = CRYSTALIZE_ERROR_UNEXPECTED_EOF;
    return NULL;
  }
  for (uint32_t index = 0; index < pointer_table_count; ++index) {
    const uint32_t buf_pos = pointer_table[index];
    if (buf_pos >= decoder.reader.size) {
      result->error = CRYSTALIZE_ERROR_POINTER_INVALID;
      return NULL;
    }
    int64_t* buf_pos_ptr = (int64_t*)(decoder.reader.buf + buf_pos);
    const int64_t relative_offset = *buf_pos_ptr;

    // add the offset to the buffer pos
    const intptr_t new_ptr_addr = (intptr_t)(decoder.reader.buf + buf_pos + relative_offset);
    if ((const char*)new_ptr_addr < decoder.reader.buf) {
      result->error = CRYSTALIZE_ERROR_POINTER_INVALID;
      return NULL;
    }
    if ((const char*)new_ptr_addr >= decoder.reader.buf + decoder.reader.size) {
      result->error = CRYSTALIZE_ERROR_POINTER_INVALID;
      return NULL;
    }
    *buf_pos_ptr = new_ptr_addr;
  }

  return data;
}
