#pragma once
#include <stdint.h>

typedef struct writer_t {
  char* buf;
  uint32_t cur;
  uint32_t capacity;
} writer_t;

void writer_ensure(writer_t* writer, uint32_t count);
void writer_pad(writer_t* writer, uint32_t count);
void writer_align(writer_t* writer, uint32_t alignment);
void writer_write(writer_t* writer, const void* data, uint32_t size);
void writer_write_u8(writer_t* writer, uint8_t value);
void writer_write_u32(writer_t* writer, uint32_t value);
