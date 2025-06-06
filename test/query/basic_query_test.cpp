#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Define test tables
struct users {
  static constexpr auto table_name = "users";
  relx::schema::column<users, "id", int> id;
  relx::schema::column<users, "name", std::string> name;
  relx::schema::column<users, "email", std::string> email;
  relx::schema::column<users, "age", int> age;
  relx::schema::column<users, "bio", std::optional<std::string>> bio;

  relx::schema::table_primary_key<&users::id> pk;
  relx::schema::unique_constraint<&users::email> unique_email;
};

struct posts {
  static constexpr auto table_name = "posts";
  relx::schema::column<posts, "id", int> id;
  relx::schema::column<posts, "user_id", int> user_id;
  relx::schema::column<posts, "title", std::string> title;
  relx::schema::column<posts, "content", std::string> content;
  relx::schema::column<posts, "created_at", std::string> created_at;

  relx::schema::table_primary_key<&posts::id> pk;
  relx::schema::foreign_key<&posts::user_id, &users::id> user_fk;
};

TEST(QueryTest, SimpleSelect) {
  users u;

  auto query = relx::query::select(u.id, u.name, u.email).from(u);

  std::string expected_sql = "SELECT users.id, users.name, users.email FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(QueryTest, SelectWithCondition) {
  users u;

  auto query = relx::query::select(u.id, u.name).from(u).where(u.age > 18);

  std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.age > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "18");
}

TEST(QueryTest, SelectWithJoin) {
  users u;
  posts p;

  auto query =
      relx::query::select(u.name, p.title).from(u).join(p, relx::query::on(u.id == p.user_id));

  std::string expected_sql =
      "SELECT users.name, posts.title FROM users JOIN posts ON (users.id = posts.user_id)";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(QueryTest, SelectWithMultipleConditions) {
  users u;

  auto query = relx::query::select(u.id, u.name).from(u).where(u.age >= 18 && u.name != "");

  std::string expected_sql =
      "SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND (users.name != ?))";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "");
}

TEST(QueryTest, SelectWithOrderByAndLimit) {
  users u;

  auto query =
      relx::query::select(u.id, u.name).from(u).order_by(relx::query::desc(u.name)).limit(10);

  std::string expected_sql =
      "SELECT users.id, users.name FROM users ORDER BY users.name DESC LIMIT ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "10");
}

TEST(QueryTest, SelectWithAggregateFunction) {
  users u;

  auto query = relx::query::select_expr(relx::query::as(relx::query::count_all(), "user_count"),
                                        relx::query::as(relx::query::avg(u.age), "average_age"))
                   .from(u);

  std::string expected_sql =
      "SELECT COUNT(*) AS user_count, AVG(users.age) AS average_age FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(QueryTest, SelectWithGroupByAndHaving) {
  users u;
  posts p;

  auto query = relx::query::select_expr(u.id,
                                        relx::query::as(relx::query::count(p.id), "post_count"))
                   .from(u)
                   .join(p, relx::query::on(u.id == p.user_id))
                   .group_by(u.id)
                   .having(relx::query::count(p.id) > 5);

  std::string expected_sql =
      "SELECT users.id, COUNT(posts.id) AS post_count FROM users JOIN posts ON (users.id = "
      "posts.user_id) GROUP BY users.id HAVING (COUNT(posts.id) > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "5");
}

TEST(QueryTest, SelectWithInCondition) {
  users u;

  std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
  auto query = relx::query::select(u.id, u.email).from(u).where(relx::query::in(u.name, names));

  std::string expected_sql =
      "SELECT users.id, users.email FROM users WHERE (users.name IN (?, ?, ?))";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "Alice");
  EXPECT_EQ(params[1], "Bob");
  EXPECT_EQ(params[2], "Charlie");
}

TEST(QueryTest, SelectWithLikeCondition) {
  users u;

  auto query =
      relx::query::select(u.id, u.name).from(u).where(relx::query::like(u.email, "%@example.com"));

  std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.email LIKE ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "%@example.com");
}

TEST(QueryTest, SelectWithCaseExpression) {
  users u;

  auto case_expr = relx::query::case_()
                       .when(u.age < 18, "Minor")
                       .when(u.age < 65, "Adult")
                       .else_("Senior")
                       .build();

  // Then check with the alias
  auto aliased_case = relx::query::as(std::move(case_expr), "age_group");

  // Finally the full query
  auto query = relx::query::select_expr(u.name, std::move(aliased_case)).from(u);

  std::string expected_sql = "SELECT users.name, CASE WHEN (users.age < ?) THEN ? WHEN (users.age "
                             "< ?) THEN ? ELSE ? END AS age_group FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();

  // The validation is valid now that we've fixed the issues
  EXPECT_EQ(params.size(), 5);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "Minor");
  EXPECT_EQ(params[2], "65");
  EXPECT_EQ(params[3], "Adult");
  EXPECT_EQ(params[4], "Senior");
}

TEST(QueryTest, SimpleCaseWithoutDuplicateParams) {
  users u;

  // Create a simple value-only condition first
  auto value_query = relx::query::select_expr(relx::query::val(42));

  auto value_params = value_query.bind_params();

  EXPECT_EQ(value_params.size(), 1);
  EXPECT_EQ(value_params[0], "42");
}

TEST(QueryTest, DebugSelectExprDuplicateParams) {
  // Test a direct value parameter
  auto single_val = relx::query::val(123);
  auto single_params = single_val.bind_params();

  // Test making a tuple with the value
  auto value_tuple = std::make_tuple(single_val);

  // Test the SelectQuery with the value directly
  auto direct_query = relx::query::SelectQuery<std::tuple<decltype(single_val)>>(
      std::make_tuple(single_val));

  auto direct_params = direct_query.bind_params();

  // Now test through the select_expr helper function
  auto select_query = relx::query::select_expr(single_val);

  auto select_params = select_query.bind_params();

  // Verify the queries work as expected
  EXPECT_EQ(single_params.size(), 1);
  EXPECT_EQ(direct_params.size(), 1);
  EXPECT_EQ(select_params.size(), 1);
}
