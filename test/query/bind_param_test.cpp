#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include <boost/uuid/string_generator.hpp>
#include <gtest/gtest.h>
#include <relx/bind_param.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

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
  ASSERT_EQ(params[0].binary.size(), 4);
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
  EXPECT_EQ(params[0].binary.size(), 8);
  EXPECT_EQ(params[0].binary[2], 0x01);  // bit 40, big-endian
}

// Regression: unsigned values once bound as the SAME-width signed OID, so
// uint32_t{4'000'000'000} arrived as -294967296. Unsigned types bind the next
// wider signed wire type; 64-bit unsigned stays untyped text.
TEST(BindParamTest, UnsignedBindsNextWiderSignedType) {
  const auto u16 = relx::make_bind_param(std::uint16_t{40000}, "40000");
  EXPECT_EQ(u16.kind, sql_kind::int4);
  ASSERT_EQ(u16.binary.size(), 4);
  EXPECT_EQ(u16.binary[0], 0x00);
  EXPECT_EQ(u16.binary[1], 0x00);
  EXPECT_EQ(u16.binary[2], 0x9C);
  EXPECT_EQ(u16.binary[3], 0x40);

  const auto u32 = relx::make_bind_param(std::uint32_t{4'000'000'000U}, "4000000000");
  EXPECT_EQ(u32.kind, sql_kind::int8);
  ASSERT_EQ(u32.binary.size(), 8);
  // 4'000'000'000 = 0x00000000EE6B2800 big-endian: positive int8, not negative int4
  EXPECT_EQ(u32.binary[0], 0x00);
  EXPECT_EQ(u32.binary[4], 0xEE);
  EXPECT_EQ(u32.binary[7], 0x00);

  const auto u8 = relx::make_bind_param(std::uint8_t{200}, "200");
  EXPECT_EQ(u8.kind, sql_kind::int2);
  ASSERT_EQ(u8.binary.size(), 2);
  EXPECT_EQ(u8.binary[0], 0x00);
  EXPECT_EQ(u8.binary[1], 0xC8);
}

TEST(BindParamTest, Unsigned64StaysUntypedText) {
  // No signed wire type can hold the full uint64_t range; the text form lets the
  // server parse it in context instead of corrupting the top bit
  const auto u64 = relx::make_bind_param(std::uint64_t{18'000'000'000'000'000'000ULL},
                                         "18000000000000000000");
  EXPECT_EQ(u64.kind, sql_kind::unspecified);
  EXPECT_TRUE(u64.binary.empty());
  EXPECT_EQ(u64.value, "18000000000000000000");
}

TEST(BindParamTest, TimePointAndDateBindTypedWithBinaryPayload) {
  using namespace std::chrono;

  // Value<time_point>/<year_month_day> once bound bare text, leaving the binary
  // timestamptz/date encoders unreached
  const auto tp = sys_days{2024y / 1 / 15} + hours{12};
  auto tp_params = relx::query::val(tp).bind_params();
  ASSERT_EQ(tp_params.size(), 1);
  EXPECT_EQ(tp_params[0].kind, sql_kind::timestamptz);
  EXPECT_EQ(tp_params[0].binary.size(), 8);
  EXPECT_EQ(tp_params[0].value, "2024-01-15T12:00:00Z");

  const year_month_day ymd = 2024y / 1 / 15;
  auto ymd_params = relx::query::val(ymd).bind_params();
  ASSERT_EQ(ymd_params.size(), 1);
  EXPECT_EQ(ymd_params[0].kind, sql_kind::date);
  EXPECT_EQ(ymd_params[0].binary.size(), 4);
  EXPECT_EQ(ymd_params[0].value, "2024-01-15");
}

TEST(BindParamTest, BoolIsTaggedBoolean) {
  auto params = relx::query::val(true).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::boolean);
  EXPECT_EQ(params[0].binary.size(), 1);
  EXPECT_EQ(params[0].binary[0], 1);
}

TEST(BindParamTest, DoubleIsTaggedFloat8WithExactBits) {
  auto params = relx::query::val(1.5).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::float8);
  EXPECT_EQ(params[0].binary.size(), 8);
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
  EXPECT_EQ(params[0].binary.size(), 0);
}

TEST(BindParamTest, EngagedOptionalIsTyped) {
  auto params = relx::query::val(std::optional<int>(7)).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::int4);
}

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
};
inline constexpr auto users = relx::t<Users>;

// clang-format on

TEST(BindParamTest, QueryParamsCarryKinds) {
  auto query = relx::query::select(users.id, users.name)
                   .from(users)
                   .where(users.id == 42 && users.name == "bob");
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

TEST(BindParamTest, UuidBindsAsBinaryOid2950) {
  boost::uuids::string_generator gen;
  const auto id = gen("6fa1cb19-5a9a-4363-9a1b-aa1b3e2c8d51");
  auto params = relx::query::val(id).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "6fa1cb19-5a9a-4363-9a1b-aa1b3e2c8d51");  // text form
  EXPECT_EQ(params[0].kind, sql_kind::uuid);
  ASSERT_EQ(params[0].binary.size(), 16);
  EXPECT_EQ(params[0].binary[0], 0x6F);  // bytes verbatim, no endian swizzle
  EXPECT_EQ(params[0].binary[1], 0xA1);
  EXPECT_EQ(params[0].binary[15], 0x51);
}

TEST(BindParamTest, ArrayParamEncodesPostgresArrayFormat) {
  auto params = relx::make_array_bind_param(std::vector<int>{1, 2});
  EXPECT_EQ(params.kind, sql_kind::int4_array);
  EXPECT_EQ(params.value, "{1,2}");  // text form is the array literal
  // header: ndims=1, hasnull=0, elem oid=23, dim=2, lbound=1, then (len=4, 1), (len=4, 2)
  ASSERT_EQ(params.binary.size(), 20 + 2 * 8);
  EXPECT_EQ(params.binary[3], 1);    // ndims
  EXPECT_EQ(params.binary[11], 23);  // int4 oid
  EXPECT_EQ(params.binary[15], 2);   // dimension length
  EXPECT_EQ(params.binary[27], 1);   // first element value
  EXPECT_EQ(params.binary[35], 2);   // second element value
}

TEST(BindParamTest, StringArrayParam) {
  auto params = relx::make_array_bind_param(std::vector<std::string>{"a", "b\"c"});
  EXPECT_EQ(params.kind, sql_kind::text_array);
  EXPECT_EQ(params.value, "{\"a\",\"b\\\"c\"}");
}

TEST(BindParamTest, InAnyConditionIsOneParam) {
  auto cond = relx::query::in_any(users.id, std::vector<int>{1, 2, 3});
  EXPECT_EQ(cond.to_sql(), "users.id = ANY(?)");
  auto params = cond.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0].kind, sql_kind::int4_array);
}

TEST(BindParamTest, InAnyIsStaticShaped) {
  auto query = relx::query::select(users.id).from(users).where(
      relx::query::in_any(users.id, std::vector<int>{1}));
  static_assert(relx::has_static_shape_v<decltype(query)>);
  SUCCEED();
}

}  // namespace
