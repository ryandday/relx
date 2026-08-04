#include <chrono>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace {

struct Sessions {
  static constexpr auto table_name = "sessions";
  relx::schema::column<Sessions, "id", boost::uuids::uuid> id;
  relx::schema::column<Sessions, "owner_id", int> owner_id;
  relx::schema::column<Sessions, "started_at", std::chrono::system_clock::time_point> started_at;
  relx::table_primary_key<&Sessions::id> pk;
};

const boost::uuids::uuid kUuid =
    boost::uuids::string_generator{}("a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11");

TEST(DeleteReturningTest, ReturningClauseAndParams) {
  Sessions s;
  auto query = relx::query::delete_from(s).where(s.owner_id == 7).returning(s.id, s.owner_id);
  EXPECT_EQ(query.to_sql(), "DELETE FROM sessions WHERE (sessions.owner_id = ?) "
                            "RETURNING sessions.id, sessions.owner_id");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "7");
}

TEST(DeleteReturningTest, ReturningBeforeWhereIsPreserved) {
  Sessions s;
  auto query = relx::query::delete_from(s).returning(s.id).where(s.owner_id == 7);
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
  Sessions s;
  auto sql = relx::create_table(s).to_sql();
  EXPECT_NE(sql.find("id UUID"), std::string::npos) << sql;
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

struct Audits {
  static constexpr auto table_name = "audits";
  relx::schema::column<Audits, "id", int, relx::schema::primary_key> id;
  relx::schema::column<Audits, "created_at", std::chrono::system_clock::time_point,
                       relx::schema::default_sql<"now()">>
      created_at;
};

TEST(DefaultSqlTest, EmitsUnquotedExpression) {
  Audits a;
  auto sql = relx::create_table(a).to_sql();
  EXPECT_NE(sql.find("created_at TIMESTAMPTZ NOT NULL DEFAULT now()"), std::string::npos) << sql;
}

}  // namespace
