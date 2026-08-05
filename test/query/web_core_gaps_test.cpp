#include <chrono>
#include <optional>
#include <string>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("sessions")]] Sessions {
  [[=relx::ann::pk]] boost::uuids::uuid id;
  int owner_id;
  std::chrono::system_clock::time_point started_at;
};
inline constexpr auto sessions = relx::t<Sessions>;

// clang-format on

const boost::uuids::uuid kUuid = boost::uuids::string_generator{}(
    "a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11");

TEST(DeleteReturningTest, ReturningClauseAndParams) {
  auto query = relx::query::delete_from(sessions)
                   .where(sessions.owner_id == 7)
                   .returning(sessions.id, sessions.owner_id);
  EXPECT_EQ(query.to_sql(), "DELETE FROM sessions WHERE (sessions.owner_id = ?) "
                            "RETURNING sessions.id, sessions.owner_id");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "7");
}

TEST(DeleteReturningTest, ReturningBeforeWhereIsPreserved) {
  auto query =
      relx::query::delete_from(sessions).returning(sessions.id).where(sessions.owner_id == 7);
  EXPECT_EQ(query.to_sql(),
            "DELETE FROM sessions WHERE (sessions.owner_id = ?) RETURNING sessions.id");
}

TEST(UuidBindTest, BindsRawUuidText) {
  auto params = relx::query::val(kUuid).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11");  // no SQL-literal quotes
}

TEST(UuidBindTest, OptionalUuidDelegates) {
  auto params = relx::query::val(std::optional<boost::uuids::uuid>(kUuid)).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11");
}

TEST(UuidBindTest, UuidColumnDdlAndRoundTrip) {
  auto sql = relx::create_table(sessions).to_sql();
  EXPECT_NE(sql.find("id UUID NOT NULL PRIMARY KEY"), std::string::npos) << sql;
  auto parsed = relx::schema::column_traits<boost::uuids::uuid>::from_sql_string(
      "a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11");
  EXPECT_EQ(parsed, kUuid);
}

TEST(ChronoBindTest, BindsUnquotedIso8601) {
  auto tp = std::chrono::system_clock::from_time_t(1700000000);  // 2023-11-14T22:13:20Z
  auto params = relx::query::val(tp).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "2023-11-14T22:13:20Z");
  EXPECT_NE(params[0].value.front(), '\'');
}

TEST(ChronoBindTest, OptionalTimePointDelegates) {
  auto tp = std::chrono::system_clock::from_time_t(1700000000);
  auto params =
      relx::query::val(std::optional<std::chrono::system_clock::time_point>(tp)).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "2023-11-14T22:13:20Z");
}

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("audits")]] Audits {
  [[=relx::ann::pk]] int id;
  [[=relx::default_sql<"now()">{}]] std::chrono::system_clock::time_point created_at;
};
inline constexpr auto audits = relx::t<Audits>;

// clang-format on

TEST(DefaultSqlTest, EmitsUnquotedExpression) {
  auto sql = relx::create_table(audits).to_sql();
  EXPECT_NE(sql.find("created_at TIMESTAMPTZ NOT NULL DEFAULT now()"), std::string::npos) << sql;
}

}  // namespace
