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
  for (int index = 0; index < value.size; ++index) {
    if ((index % 16) == 0) {
      if (index != 0) {
        os << "  ";
        for (int ascii_index = 15; ascii_index >= 0; --ascii_index) {
          char c = value.buf[index - ascii_index];
          if (c >= 0x20 && c <= 0x7e) {
            os << c;
          }
          else {
            os << '.';
          }
        }
        os << std::endl;
      }
      os << std::hex << std::setfill('0') << std::setw(8) << (unsigned int)index << ":";
    }
    if (index % 2 == 0) {
      os << " ";
    }
    os << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)((unsigned char*)value.buf)[index];
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
    crystalize_schema_field_init(fields + 0, "a", CRYSTALIZE_BOOL, 1);
    crystalize_schema_field_init(fields + 1, "b", CRYSTALIZE_INT32, 3);
    crystalize_schema_init(&schema, "simple", 0, fields, 2);

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
    crystalize_schema_t schema;
    crystalize_schema_field_t fields[2];
    crystalize_schema_field_init(fields + 0, "a", CRYSTALIZE_BOOL, 1);
    crystalize_schema_field_init(fields + 1, "b", CRYSTALIZE_INT32, 3);
    crystalize_schema_init(&schema, "simple", 0, fields, 2);
    crystalize_schema_add(&schema);

    struct simple_t {
      bool a;
      int32_t b[3];
    };
    simple_t data;
    data.a = true;
    data.b[0] = 1;
    data.b[1] = 2;
    data.b[2] = 3;

    unsigned char expected[] = {
      0x63, 0x72, 0x79, 0x73, // magic
      0x01, 0x00, 0x00, 0x00, // encoding version
      0x01, 0x00, 0x00, 0x00, // endian
      0x01, 0x00, 0x00, 0x00, // schema count

      0x7f, 0x80, 0x66, 0x16, // fnv1a("simple")
      0x00, 0x00, 0x00, 0x00, // schema version
      0x02, 0x00, 0x00, 0x00, // schema field count
      0x00, 0x00, 0x00, 0x00, // (pad for alignment)
      0x08, 0x00, 0x00, 0x00, // schema fields pointer offset (8 bytes)
      0x00, 0x00, 0x00, 0x00, // (more pointer)

      0x2c, 0x29, 0x0c, 0xe4, // field0: fnv1a("a")
      0x01, 0x00, 0x00, 0x00, // field0: count
      0x00, 0x00, 0x00, 0x00, // field0: type (bool) + pad

      0xe5, 0x2d, 0x0c, 0xe7, // field1: fnv1a("b")
      0x03, 0x00, 0x00, 0x00, // field1: count
      0x04, 0x00, 0x00, 0x00, // field1: type(int32) + pad

      0x01, 0x00, 0x00, 0x00, // true + pad
      0x01, 0x00, 0x00, 0x00, // int32: 1
      0x02, 0x00, 0x00, 0x00, // int32: 2
      0x03, 0x00, 0x00, 0x00, // int32: 3
    };
    buf_t buf_expected;
    buf_expected.buf = (char*)expected;
    buf_expected.size = sizeof(expected);

    buf_t buf_result;
    crystalize_encode(schema.name_id, &data, &buf_result.buf, &buf_result.size);
    CHECK(buf_expected == buf_result);
  }
}
