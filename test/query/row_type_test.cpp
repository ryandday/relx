#include <optional>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string username;
  std::optional<std::string> bio;
  double score;
};
inline constexpr auto users = relx::t<Users>;

TEST(RowTypeTest, PlainColumnsSynthesizeMatchingMembers) {
  auto q = relx::query::select(users.id, users.username, users.bio).from(users);
  using Row = relx::row_type_for<decltype(q)>;

  static_assert(std::is_same_v<decltype(Row::id), int>);
  static_assert(std::is_same_v<decltype(Row::username), std::string>);
  static_assert(std::is_same_v<decltype(Row::bio), std::optional<std::string>>);

  Row row{.id = 7, .username = "jane", .bio = std::nullopt};
  EXPECT_EQ(row.id, 7);
  EXPECT_EQ(row.username, "jane");
  EXPECT_FALSE(row.bio.has_value());
}

TEST(RowTypeTest, TypedAliasDeducesFromExpression) {
  auto q = relx::query::select(relx::as<"who">(users.username)).from(users);
  using Row = relx::row_type_for<decltype(q)>;
  static_assert(std::is_same_v<decltype(Row::who), std::string>);
  EXPECT_EQ(q.to_sql(), "SELECT users.username AS who FROM users");
}

TEST(RowTypeTest, TypedAliasWithExplicitTypeForAggregates) {
  auto q = relx::query::select(users.username,
                               relx::as<"post_count", long>(relx::count(users.id)))
               .from(users);
  using Row = relx::row_type_for<decltype(q)>;
  static_assert(std::is_same_v<decltype(Row::post_count), long>);

  EXPECT_EQ(q.to_sql(), "SELECT users.username, COUNT(users.id) AS post_count FROM users");

  Row row{.username = "bob", .post_count = 42};
  EXPECT_EQ(row.post_count, 42);
}

TEST(RowTypeTest, RowFieldCountMatchesSelectList) {
  auto q3 = relx::query::select(users.id, users.username, users.score).from(users);
  static_assert(relx::refl::field_count<relx::row_type_for<decltype(q3)>>() == 3);

  auto q1 = relx::query::select(users.id).from(users);
  static_assert(relx::refl::field_count<relx::row_type_for<decltype(q1)>>() == 1);
}

// clang-format on

}  // namespace
