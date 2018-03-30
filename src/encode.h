#pragma once
#include <stdint.h>

typedef struct crystalize_schema_t crystalize_schema_t;

void encoder_encode(const crystalize_schema_t* schema, const void* data, char** buf, uint32_t* buf_size);
