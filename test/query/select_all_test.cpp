#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Define test tables
// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
  [[=relx::ann::unique]] std::string email;
  int age;
  std::string created_at;
  bool is_active;
  std::optional<std::string> bio;
  int login_count;
};
constexpr auto users = relx::t<Users>;

// Define a second table
struct [[=relx::table("posts")]] Posts {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  std::string title;
  std::string content;
};
constexpr auto posts = relx::t<Posts>;

}  // namespace

// clang-format on

TEST(SelectAllTest, BasicSelectAll) {
  // Use select_all with a table object
  auto query = relx::query::select_all(users);

  // The expected SQL should include all columns but not constraints
  std::string expected_sql = "SELECT users.id, users.name, users.email, users.age, "
                             "users.created_at, users.is_active, users.bio, "
                             "users.login_count FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithoutInstance) {
  // Use select_all with just the table type
  auto query = relx::query::select_all<Users>();

  // The expected SQL should include all columns but not constraints
  std::string expected_sql = "SELECT users.id, users.name, users.email, users.age, "
                             "users.created_at, users.is_active, users.bio, "
                             "users.login_count FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithWhere) {
  auto query = relx::query::select_all<Users>().where(users.age > 18);

  std::string expected_sql = "SELECT users.id, users.name, users.email, users.age, "
                             "users.created_at, users.is_active, users.bio, "
                             "users.login_count FROM users WHERE (users.age > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "18");
}

TEST(SelectAllTest, SelectAllWithJoin) {
  auto query = relx::query::select_all<Users>().join(posts,
                                                     relx::query::on(users.id == posts.user_id));

  std::string expected_sql =
      "SELECT users.id, users.name, users.email, users.age, "
      "users.created_at, users.is_active, users.bio, "
      "users.login_count FROM users JOIN posts ON (users.id = posts.user_id)";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithAllClauses) {
  auto query = relx::query::select_all<Users>()
                   .join(posts, relx::query::on(users.id == posts.user_id))
                   .where(users.age > 18)
                   .group_by(users.id)
                   .having(relx::query::count(posts.id) > 5)
                   .order_by(relx::query::desc(users.age))
                   .limit(10)
                   .offset(20);

  std::string expected_sql = "SELECT users.id, users.name, users.email, users.age, "
                             "users.created_at, users.is_active, users.bio, "
                             "users.login_count FROM users "
                             "JOIN posts ON (users.id = posts.user_id) "
                             "WHERE (users.age > ?) "
                             "GROUP BY users.id "
                             "HAVING (COUNT(posts.id) > ?) "
                             "ORDER BY users.age DESC "
                             "LIMIT ? "
                             "OFFSET ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "5");
  EXPECT_EQ(params[2], "10");
  EXPECT_EQ(params[3], "20");
}

TEST(SelectAllTest, SynthesizesRowType) {
  // The whole point of expanding * to explicit columns: the select list is typed,
  // so the query synthesizes a row struct like any explicit select
  auto query = relx::query::select_all(users);
  using Row = relx::query::row_type_for<decltype(query)>;

  Row row{};
  row.id = 7;
  row.name = "alice";
  row.bio = std::nullopt;
  EXPECT_EQ(row.id, 7);
  EXPECT_EQ(row.name, "alice");
  EXPECT_FALSE(row.bio.has_value());
}
