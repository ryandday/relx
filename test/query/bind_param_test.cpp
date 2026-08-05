#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/bind_param.hpp>
#include <relx/query.hpp>

using relx::bind_param;
using relx::sql_kind;

namespace {

// Typed values bound through the query builder carry their SQL kind and the exact
// big-endian wire bytes; strings and enums stay untyped text.

TEST(BindParamTest, IntParamIsTaggedInt4) {
  auto v = relx::query::val(42);
  auto params = v.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "42");  // text form preserved
  EXPECT_EQ(params[0].kind, sql_kind::int4);
  ASSERT_EQ(params[0].binary_size, 4);
  EXPECT_EQ(params[0].binary[0], 0x00);
  EXPECT_EQ(params[0].binary[1], 0x00);
  EXPECT_EQ(params[0].binary[2], 0x00);
  EXPECT_EQ(params[0].binary[3], 0x2A);
}

TEST(BindParamTest, NegativeIntEncodesTwosComplement) {
  auto params = relx::query::val(-1).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::int4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(params[0].binary[i], 0xFF);
  }
}

TEST(BindParamTest, LongLongIsTaggedInt8) {
  auto params = relx::query::val(1LL << 40).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::int8);
  EXPECT_EQ(params[0].binary_size, 8);
  EXPECT_EQ(params[0].binary[2], 0x01);  // bit 40, big-endian
}

TEST(BindParamTest, BoolIsTaggedBoolean) {
  auto params = relx::query::val(true).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::boolean);
  EXPECT_EQ(params[0].binary_size, 1);
  EXPECT_EQ(params[0].binary[0], 1);
}

TEST(BindParamTest, DoubleIsTaggedFloat8WithExactBits) {
  auto params = relx::query::val(1.5).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::float8);
  EXPECT_EQ(params[0].binary_size, 8);
  // 1.5 == 0x3FF8000000000000
  EXPECT_EQ(params[0].binary[0], 0x3F);
  EXPECT_EQ(params[0].binary[1], 0xF8);
  EXPECT_EQ(params[0].binary[2], 0x00);
}

TEST(BindParamTest, StringStaysUntypedText) {
  auto params = relx::query::val("hello").bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "hello");
  EXPECT_EQ(params[0].kind, sql_kind::unspecified);
  EXPECT_EQ(params[0].binary_size, 0);
}

TEST(BindParamTest, EngagedOptionalIsTyped) {
  auto params = relx::query::val(std::optional<int>(7)).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::int4);
}

struct users {
  static constexpr auto table_name = "users";
  relx::schema::column<users, "id", int> id;
  relx::schema::column<users, "name", std::string> name;
};

TEST(BindParamTest, QueryParamsCarryKinds) {
  users u;
  auto query = relx::query::select(u.id, u.name).from(u).where(u.id == 42 && u.name == "bob");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "42");
  EXPECT_EQ(params[0].kind, sql_kind::int4);
  EXPECT_EQ(params[1], "bob");
  EXPECT_EQ(params[1].kind, sql_kind::unspecified);
}

TEST(BindParamTest, StringComparisonCompatibility) {
  // The compatibility surface hundreds of existing call sites rely on
  bind_param p = std::string("abc");
  EXPECT_EQ(p, "abc");
  EXPECT_EQ(p, std::string("abc"));
  EXPECT_EQ(p, std::string_view("abc"));
  std::vector<bind_param> params{p};
  EXPECT_TRUE(params == std::vector<std::string>{"abc"});
}

TEST(BindParamTest, OptionalStringBindsRawText) {
  auto params = relx::query::val(std::optional<std::string>("it's raw")).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "it's raw");  // no SQL-literal quoting in bound text
  EXPECT_EQ(params[0].kind, sql_kind::unspecified);
}

TEST(BindParamTest, DisengagedOptionalBindsTypedNull) {
  auto v = relx::query::val(std::optional<int>());
  EXPECT_EQ(v.to_sql(), "?");  // SQL shape is value-independent
  auto params = v.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_TRUE(params[0].is_null);
  EXPECT_EQ(params[0].kind, sql_kind::int4);  // NULL still carries its type
}

TEST(BindParamTest, BareNulloptBindsUntypedNull) {
  auto params = relx::query::val(std::nullopt).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_TRUE(params[0].is_null);
  EXPECT_EQ(params[0].kind, sql_kind::unspecified);
}

}  // namespace
