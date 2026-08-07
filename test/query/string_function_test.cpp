#include "test_common.hpp"

#include <gtest/gtest.h>

using namespace test_tables;
using namespace test_utils;

TEST(StringFunctionTest, Lower) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(
                   u.id, relx::query::as(relx::query::lower(u.name), "lowercase_name"))
                   .from(u);

  std::string expected_sql = "SELECT users.id, LOWER(users.name) AS lowercase_name FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(StringFunctionTest, Upper) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(
                   u.id, relx::query::as(relx::query::upper(u.name), "uppercase_name"))
                   .from(u);

  std::string expected_sql = "SELECT users.id, UPPER(users.name) AS uppercase_name FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(StringFunctionTest, Length) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(u.name,
                                        relx::query::as(relx::query::length(u.name), "name_length"))
                   .from(u);

  std::string expected_sql = "SELECT users.name, LENGTH(users.name) AS name_length FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(StringFunctionTest, Trim) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(u.id,
                                        relx::query::as(relx::query::trim(u.name), "trimmed_name"))
                   .from(u);

  std::string expected_sql = "SELECT users.id, TRIM(users.name) AS trimmed_name FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(StringFunctionTest, StringFunctionInWhere) {
  constexpr auto u = users;

  auto query = relx::query::select(u.id, u.name)
                   .from(u)
                   .where(relx::query::upper(u.email) == "EMAIL@EXAMPLE.COM");

  std::string expected_sql =
      "SELECT users.id, users.name FROM users WHERE (UPPER(users.email) = ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "EMAIL@EXAMPLE.COM");
}

TEST(StringFunctionTest, LengthInCondition) {
  constexpr auto u = users;

  auto query = relx::query::select(u.id, u.name).from(u).where(relx::query::length(u.name) > 5);

  std::string expected_sql =
      "SELECT users.id, users.name FROM users WHERE (LENGTH(users.name) > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "5");
}

TEST(StringFunctionTest, CombinedStringFunctions) {
  constexpr auto u = users;

  auto query = relx::query::select(u.id, u.name)
                   .from(u)
                   .where(relx::query::length(relx::query::trim(relx::query::lower(u.email))) > 10);

  std::string expected_sql =
      "SELECT users.id, users.name FROM users WHERE (LENGTH(TRIM(LOWER(users.email))) > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "10");
}

TEST(StringFunctionTest, StringFunctionInOrderBy) {
  constexpr auto u = users;

  auto query = relx::query::select(u.id, u.name).from(u).order_by(relx::query::length(u.name));

  std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY LENGTH(users.name)";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(StringFunctionTest, StringFunctionInGroupBy) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(relx::query::upper(u.name),
                                        relx::query::as(relx::query::count_all(), "count"))
                   .from(u)
                   .group_by(relx::query::upper(u.name));

  std::string expected_sql =
      "SELECT UPPER(users.name), COUNT(*) AS count FROM users GROUP BY UPPER(users.name)";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(StringFunctionTest, Coalesce) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(
                   u.id, relx::query::as(relx::query::coalesce(u.bio, "No biography"), "biography"))
                   .from(u);

  std::string expected_sql = "SELECT users.id, COALESCE(users.bio, ?) AS biography FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "No biography");
}

// Regression: CoalesceExpr::bind_params once built its result from iterators of two
// distinct temporary vectors, corrupting memory for 3+ argument coalesce with params
TEST(StringFunctionTest, CoalesceMultipleValues) {
  constexpr auto u = users;

  auto query = relx::query::select_expr(
                   u.id,
                   relx::query::as(relx::query::coalesce(u.bio, u.name, "Unknown"), "display_text"))
                   .from(u);

  std::string expected_sql =
      "SELECT users.id, COALESCE(users.bio, users.name, ?) AS display_text FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "Unknown");
}

TEST(StringFunctionTest, CoalesceManyParamsInRest) {
  constexpr auto u = users;

  auto expr = relx::query::coalesce(u.bio, relx::query::val("a"), relx::query::val("b"),
                                    relx::query::val("c"), relx::query::val("d"));
  EXPECT_EQ(expr.to_sql(), "COALESCE(users.bio, ?, ?, ?, ?)");

  auto params = expr.bind_params();
  ASSERT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "a");
  EXPECT_EQ(params[1], "b");
  EXPECT_EQ(params[2], "c");
  EXPECT_EQ(params[3], "d");
}

TEST(StringFunctionTest, CoalesceInWhere) {
  constexpr auto u = users;

  auto query =
      relx::query::select(u.id, u.name).from(u).where(relx::query::coalesce(u.bio, "") != "");

  std::string expected_sql =
      "SELECT users.id, users.name FROM users WHERE (COALESCE(users.bio, ?) != ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "");
  EXPECT_EQ(params[1], "");
}