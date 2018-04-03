#pragma once
#include <stdint.h>

#define CRYSTALIZE_FILE_VERSION 0u

typedef struct crystalize_schema_t crystalize_schema_t;
typedef struct crystalize_encode_result_t crystalize_encode_result_t;

void encoder_encode(const crystalize_schema_t* schema, const void* data, crystalize_encode_result_t* result);
void* encoder_decode(const crystalize_schema_t* schema, char* buf, uint32_t buf_size);
