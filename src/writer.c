#include <string.h>
#include "writer.h"
#include "config.h"

void writer_ensure(writer_t* writer, uint32_t count) {
  uint32_t new_cur = writer->cur + count;
  if (new_cur > writer->capacity) {
    // uint32_t old_capacity = writer->capacity;
    writer->capacity = (new_cur + 1023) & ~1023;
    writer->buf = (char*)crystalize_realloc(writer->buf, writer->capacity);
    // memset(writer->buf + old_capacity, 0xcc, (writer->capacity - old_capacity));
  }
}

void writer_pad(writer_t* writer, uint32_t count) {
  writer_ensure(writer, count);
  for (uint32_t index = 0; index < count; ++index) {
    writer->buf[writer->cur + index] = 0;
  }
  writer->cur += count;
}

void writer_align(writer_t* writer, uint32_t alignment) {
  uint32_t new_cur = (writer->cur + (alignment - 1)) & ~(alignment - 1);
  if (new_cur > writer->cur) {
    writer_pad(writer, (new_cur - writer->cur));
  }
}

void writer_write(writer_t* writer, const void* data, uint32_t size) {
  writer_ensure(writer, size);
  memmove(writer->buf + writer->cur, data, size);
  writer->cur += size;
}

void writer_write_u8(writer_t* writer, uint8_t value) {
  writer_align(writer, 1);
  writer_write(writer, &value, 1);
}

void writer_write_u32(writer_t* writer, uint32_t value) {
  writer_align(writer, 4);
  writer_write(writer, &value, 4);
}
