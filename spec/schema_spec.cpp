#include "catch.hpp"
#include "crystalize.h"

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

TEST_CASE("schema registration") {
  init_t init(nullptr);

  SECTION("it properly fails to find schemas that don't exist") {
    CHECK(crystalize_schema_get(0) == NULL);
  }

  SECTION("it allows an empty schema") {
    crystalize_schema_t schema_empty;
    crystalize_schema_init(&schema_empty, "empty", 0, NULL, 0);
    crystalize_schema_add(&schema_empty);
    CHECK(crystalize_schema_get(schema_empty.name_id) != NULL);
  }

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

  SECTION("it encodes an empty struct") {
    crystalize_schema_t schema;
    crystalize_schema_init(&schema, "empty", 0, NULL, 0);
  }
}
