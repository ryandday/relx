#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/json.hpp>
#include <relx/schema.hpp>

// Struct-typed JSONB columns: reflection-driven encode/decode + column integration

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

struct [[=relx::ann::jsonb]] Geo {
  double lat;
  double lon;
};

struct [[=relx::ann::jsonb]] Meta {
  std::string device;
  int version;
  bool beta;
  std::optional<std::string> note;
  std::vector<int> tags;
  Geo geo;
};

struct [[=relx::table("jt_events")]] JtEvent {
  [[=relx::ann::pk]] int id;
  Meta metadata;
  std::optional<Geo> last_location;
};

struct [[=relx::ann::jsonb]] JtText {
  std::string value;
};

}  // namespace

// clang-format on

TEST(JsonbTest, EncodeProducesCompactJson) {
  const Meta meta{.device = "kiosk \"7\"\n",
                  .version = 3,
                  .beta = true,
                  .note = std::nullopt,
                  .tags = {1, 2, 3},
                  .geo = {.lat = 52.5, .lon = 13.4}};
  EXPECT_EQ(relx::json::to_json(meta),
            R"({"device":"kiosk \"7\"\n","version":3,"beta":true,"note":null,)"
            R"("tags":[1,2,3],"geo":{"lat":52.5,"lon":13.4}})");
}

TEST(JsonbTest, RoundTripPreservesEverything) {
  const Meta meta{.device = "d",
                  .version = 1,
                  .beta = false,
                  .note = "hello",
                  .tags = {},
                  .geo = {.lat = -1.25, .lon = 0.0}};
  auto back = relx::json::from_json<Meta>(relx::json::to_json(meta));
  ASSERT_TRUE(back) << back.error();
  EXPECT_EQ(back->device, "d");
  EXPECT_EQ(back->note, "hello");
  EXPECT_TRUE(back->tags.empty());
  EXPECT_EQ(back->geo.lat, -1.25);
}

TEST(JsonbTest, DecodeIsKeyOrderIndependent) {
  // JSONB normalizes key order server-side; decoding must not depend on it
  auto geo = relx::json::from_json<Geo>(R"({"lon":1.5,"lat":2.5})");
  ASSERT_TRUE(geo) << geo.error();
  EXPECT_EQ(geo->lat, 2.5);
  EXPECT_EQ(geo->lon, 1.5);
}

TEST(JsonbTest, DecodeIsStrict) {
  EXPECT_FALSE(relx::json::from_json<Geo>(R"({"lat":1,"lon":2,"extra":3})"));
  EXPECT_FALSE(relx::json::from_json<Geo>(R"({"lat":1})"));
  EXPECT_FALSE(relx::json::from_json<Geo>(R"({"lat":1,"lon":2} trailing)"));
  EXPECT_FALSE(relx::json::from_json<Geo>(R"({"lat":"nope","lon":2})"));
  // a number where optional<string> is expected is an error, not a coercion
  EXPECT_FALSE(relx::json::from_json<Meta>(
      R"({"device":"d","version":1,"beta":false,"note":42,"tags":[],"geo":{"lat":0,"lon":0}})"));
  // null lands only in optionals
  EXPECT_FALSE(relx::json::from_json<Geo>(R"({"lat":null,"lon":2})"));
}

TEST(JsonbTest, UnicodeEscapesDecode) {
  auto decoded = relx::json::from_json<JtText>(R"({"value":"aé€😀b"})");
  ASSERT_TRUE(decoded) << decoded.error();
  EXPECT_EQ(decoded->value, "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"
                            "b");
}

TEST(JsonbTest, ColumnTraitsIntegration) {
  static_assert(relx::json::JsonbAnnotated<Meta>);
  static_assert(!relx::json::JsonbAnnotated<int>);
  static_assert(std::string_view(relx::schema::column_traits<Meta>::sql_type_name) == "JSONB");

  const Meta meta{.device = "d",
                  .version = 1,
                  .beta = false,
                  .note = std::nullopt,
                  .tags = {9},
                  .geo = {.lat = 1, .lon = 2}};
  const std::string encoded = relx::schema::column_traits<Meta>::to_sql_string(meta);
  const Meta decoded = relx::schema::column_traits<Meta>::from_sql_string(encoded);
  EXPECT_EQ(decoded.tags, std::vector<int>{9});
  EXPECT_THROW(relx::schema::column_traits<Meta>::from_sql_string("not json"), std::runtime_error);
}

TEST(JsonbTest, ColumnDefinitionUsesJsonbType) {
  const relx::schema::column<relx::schema::table_t<JtEvent>, "metadata", Meta> column{};
  EXPECT_EQ(column.sql_definition(), "metadata JSONB NOT NULL");

  const relx::schema::column<relx::schema::table_t<JtEvent>, "last_location", std::optional<Geo>>
      optional_column{};
  EXPECT_EQ(optional_column.sql_definition(), "last_location JSONB");
}

TEST(JsonbTest, AnnotatedTableDdlIncludesJsonbColumns) {
  constexpr std::string_view ddl = relx::create_table_sql<JtEvent>().to_sql();
  EXPECT_NE(ddl.find("metadata JSONB NOT NULL"), std::string_view::npos) << ddl;
  EXPECT_NE(ddl.find("last_location JSONB"), std::string_view::npos) << ddl;
}
