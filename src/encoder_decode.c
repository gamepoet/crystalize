#include "crystalize.h"
#include "encoder.h"
#include <stdalign.h>
#include <string.h>
#include <stdio.h>

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
  read_bytes(reader, val, 4);
}

void* encoder_decode(const crystalize_schema_t* schema, char* buf, uint32_t buf_size) {
  decoder_t decoder = {{0}};
  decoder.reader.buf = buf;
  decoder.reader.size = buf_size;
  decoder.reader.cur = 0;
  decoder.reader.error = NULL;

  // read the file header
  uint8_t magic[4];
  uint32_t file_version;
  uint32_t endian;
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
  read_u32(&decoder.reader, &data_offset);
  read_u32(&decoder.reader, &pointer_table_offset);
  read_u32(&decoder.reader, &pointer_table_count);
  read_u32(&decoder.reader, &schema_count);
  if (decoder.reader.error) {
    return NULL;
  }
  if (magic[0] != 0x63 || magic[1] != 0x72 || magic[2] != 0x79 || magic[3] != 0x73) {
    printf("magic mismatch\n");
    return NULL;
  }
  if (file_version != CRYSTALIZE_FILE_VERSION) {
    printf("file_version mismatch\n");
    return NULL;
  }
  if (endian != 0x01u) {
    printf("endian mismatch\n");
    return NULL;
  }
  if (data_offset >= buf_size) {
    // offset to data start is invalid
    printf("data offset mismatch\n");
    return NULL;
  }

  // load the schema table
  read_align(&decoder.reader, alignof(crystalize_schema_t));
  const crystalize_schema_t* schemas = (const crystalize_schema_t*)read_pos(&decoder.reader);
  read_consume(&decoder.reader, schema_count * sizeof(crystalize_schema_t));
  if (decoder.reader.error) {
    return NULL;
  }

  // TODO: handle mismatched schemas
  schemas = NULL;

  void* data = decoder.reader.buf + data_offset;

  // TODO: fixup pointers
  uint32_t* pointer_table = (uint32_t*)(decoder.reader.buf + pointer_table_offset);
  for (uint32_t index = 0; index < pointer_table_count; ++index) {
    const uint32_t buf_pos = pointer_table[index];
    int64_t* buf_pos_ptr = (int64_t*)(decoder.reader.buf + buf_pos);
    const int64_t offset = *buf_pos_ptr;

    // add the offset to the buffer pos
    const int64_t new_ptr_addr = buf_pos + offset;
    *buf_pos_ptr = new_ptr_addr;
  }

  return data;
}
