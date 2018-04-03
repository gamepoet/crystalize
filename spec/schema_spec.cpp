#include "catch.hpp"
#include "crystalize.h"
#include <iostream>
#include <iomanip>

// init/shutdown helper if an exception gets thrown
struct init_t {
  init_t(const crystalize_config_t* config) {
    crystalize_config_t config_default;
    if (!config) {
      crystalize_config_init(&config_default);
      config = &config_default;
    }
    crystalize_init(config);
  }
  ~init_t() {
    crystalize_shutdown();
  }
};

struct buf_t {
  char* buf;
  uint32_t size;
  uint32_t capacity;

  bool operator==(const buf_t& other) const {
    if (size != other.size) {
      return false;
    }
    return 0 == memcmp(buf, other.buf, size);
  }
};

std::ostream& operator<<(std::ostream& os, const buf_t& value) {
  for (uint32_t offset = 0; offset < value.size; offset += 16) {
    uint32_t count = value.size - offset;
    if (count > 16) {
      count = 16;
    }

    os << std::hex << std::setfill('0') << std::setw(8) << (unsigned int)offset << ":";
    uint32_t index;
    for (index = 0; index < count; ++index) {
      if (index % 2 == 0) {
        os << " ";
      }
      os << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)((unsigned char*)value.buf)[offset + index];
    }
    for (; index < 16; ++index) {
      if (index % 2 == 0) {
        os << " ";
      }
      os << "  ";
    }
    os << "  ";
    for (index = 0; index < count; ++index) {
      char c = value.buf[offset + index];
      if (c >= 0x20 && c <= 0x7e) {
        os << c;
      }
      else {
        os << '.';
      }
    }
    for (; index < 16; ++index) {
      os << ' ';
    }
    os << std::endl;
  }
	return os;
}

TEST_CASE("schema registration") {
  init_t init(nullptr);

  SECTION("it properly fails to find schemas that don't exist") {
    CHECK(crystalize_schema_get(0) == NULL);
  }

  // SECTION("it disallows an empty schema") {
  //   crystalize_schema_t schema_empty;
  //   crystalize_schema_init(&schema_empty, "empty", 0, NULL, 0);
  //   crystalize_schema_add(&schema_empty);
  //   CHECK(crystalize_schema_get(schema_empty.name_id) != NULL);
  // }

  SECTION("it allows a schema with fields") {
    crystalize_schema_t schema;
    crystalize_schema_field_t fields[2];
    crystalize_schema_field_init_scalar(fields + 0, "a", CRYSTALIZE_BOOL, 1);
    crystalize_schema_field_init_scalar(fields + 1, "b", CRYSTALIZE_INT32, 3);
    crystalize_schema_init(&schema, "simple", 0, 4, fields, 2);

    crystalize_schema_add(&schema);
    const crystalize_schema_t* found = crystalize_schema_get(schema.name_id);
    CHECK(found != NULL);
    CHECK(found->field_count == 2);
    CHECK(found->fields[0].type == CRYSTALIZE_BOOL);
    CHECK(found->fields[0].count == 1);
    CHECK(found->fields[1].type == CRYSTALIZE_INT32);
    CHECK(found->fields[1].count == 3);
  }
}

TEST_CASE("encoding") {
  init_t init(nullptr);

  SECTION("it encodes a struct") {
    struct simple_t {
      bool a;
      int32_t b[3];
    };

    crystalize_schema_t schema;
    crystalize_schema_field_t fields[2];
    crystalize_schema_field_init_scalar(fields + 0, "a", CRYSTALIZE_BOOL, 1);
    crystalize_schema_field_init_scalar(fields + 1, "b", CRYSTALIZE_INT32, 3);
    crystalize_schema_init(&schema, "simple", 0, alignof(simple_t), fields, 2);
    crystalize_schema_add(&schema);

    simple_t data;
    data.a = true;
    data.b[0] = 1;
    data.b[1] = 2;
    data.b[2] = 3;

    unsigned char expected[] = {
      0x63, 0x72, 0x79, 0x73, // magic
      0x00, 0x00, 0x00, 0x00, // encoding version
      0x01, 0x00, 0x00, 0x00, // endian
      0x60, 0x00, 0x00, 0x00, // offset to the data section from the start of the file
      0x70, 0x00, 0x00, 0x00, // offset to the pointer fixup table
      0x01, 0x00, 0x00, 0x00, // pointer fixup count
      0x01, 0x00, 0x00, 0x00, // schema count

      0x00, 0x00, 0x00, 0x00, // pad for alignment

      0x7f, 0x80, 0x66, 0x16, // fnv1a("simple")
      0x00, 0x00, 0x00, 0x00, // schema version
      0x04, 0x00, 0x00, 0x00, // alignment
      0x02, 0x00, 0x00, 0x00, // schema field count
      0x08, 0x00, 0x00, 0x00, // schema fields pointer relative offset (8 bytes)
      0x00, 0x00, 0x00, 0x00, // (more pointer)

      0x2c, 0x29, 0x0c, 0xe4, // field0: fnv1a("a")
      0x00, 0x00, 0x00, 0x00, // field0: struct_name_id
      0x01, 0x00, 0x00, 0x00, // field0: count
      0x00, 0x00, 0x00, 0x00, // field0: count_field_name_id
      0x00, 0x00, 0x00, 0x00, // field1: type(bool) + pad

      0xe5, 0x2d, 0x0c, 0xe7, // field1: fnv1a("b")
      0x00, 0x00, 0x00, 0x00, // field0: struct_name_id
      0x03, 0x00, 0x00, 0x00, // field1: count
      0x00, 0x00, 0x00, 0x00, // field0: count_field_name_id
      0x04, 0x00, 0x00, 0x00, // field1: type(int32) + pad

      0x01, 0x00, 0x00, 0x00, // true + pad
      0x01, 0x00, 0x00, 0x00, // int32: 1
      0x02, 0x00, 0x00, 0x00, // int32: 2
      0x03, 0x00, 0x00, 0x00, // int32: 3

      0x30, 0x00, 0x00, 0x00, // pointer table
    };
    buf_t buf_expected;
    buf_expected.buf = (char*)expected;
    buf_expected.size = sizeof(expected);

    buf_t buf_result;
    crystalize_encode(schema.name_id, &data, &buf_result.buf, &buf_result.size);
    CHECK(buf_result == buf_expected);

    // decode it back
    simple_t* decoded = (simple_t*)crystalize_decode(schema.name_id, buf_result.buf, buf_result.size);
    CHECK(decoded != NULL);
    CHECK(decoded->a == true);
    CHECK(decoded->b[0] == 1);
    CHECK(decoded->b[1] == 2);
    CHECK(decoded->b[2] == 3);
  }

  SECTION("it encodes a struct with scalar pointers") {
    struct root_t {
      char a;
      int16_t b_count;
      float* b;
    };
    crystalize_schema_t schema;
    crystalize_schema_field_t fields[3];
    crystalize_schema_field_init_scalar(fields + 0, "a", CRYSTALIZE_CHAR, 1);
    crystalize_schema_field_init_scalar(fields + 1, "b_count", CRYSTALIZE_INT16, 1);
    crystalize_schema_field_init_counted_scalar(fields + 2, "b", CRYSTALIZE_FLOAT, "b_count");
    crystalize_schema_init(&schema, "root", 0, alignof(root_t), fields, 3);
    crystalize_schema_add(&schema);

    float values[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    root_t data;
    data.a = 'a';
    data.b_count = 4;
    data.b = values;

    // encode it
    buf_t buf_result;
    crystalize_encode(schema.name_id, &data, &buf_result.buf, &buf_result.size);
    CHECK(buf_result.size != 0);

    // decode it back
    root_t* decoded = (root_t*)crystalize_decode(schema.name_id, buf_result.buf, buf_result.size);
    CHECK(decoded != NULL);
    CHECK(decoded->a == 'a');
    CHECK(decoded->b_count == 4);
    CHECK(decoded->b[0] == 1.0f);
    CHECK(decoded->b[1] == 2.0f);
    CHECK(decoded->b[2] == 3.0f);
    CHECK(decoded->b[3] == 4.0f);
  }
}
