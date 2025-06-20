#include "test_common.hpp"

#include <gtest/gtest.h>

using namespace test_tables;
using namespace test_utils;

/// Test cases for the SELECT DISTINCT functionality

TEST(DistinctSelectTest, SimpleSelectDistinctLegacy) {
  users u;

  auto query = relx::query::select_distinct(u.id, u.name, u.email).from(u);

  EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.id, users.name, users.email FROM users");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithCondition) {
  users u;
  auto query = relx::query::select_distinct(u.id, u.name).from(u).where(u.age > 18);

  EXPECT_EQ(query.to_sql(),
            "SELECT DISTINCT users.id, users.name FROM users WHERE (users.age > ?)");

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "18");
}

TEST(DistinctSelectTest, SelectDistinctWithJoin) {
  // Test DISTINCT with JOIN
  users u;
  posts p;
  auto query = relx::query::select_distinct(u.id, p.title)
                   .from(u)
                   .join(p, relx::query::on(u.id == p.user_id));

  EXPECT_EQ(
      query.to_sql(),
      "SELECT DISTINCT users.id, posts.title FROM users JOIN posts ON (users.id = posts.user_id)");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithGroupBy) {
  users u;
  auto query = relx::query::select_distinct(u.name, u.age).from(u).group_by(u.age);

  EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.name, users.age FROM users GROUP BY users.age");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithOrderBy) {
  users u;
  auto query =
      relx::query::select_distinct(u.name, u.age).from(u).order_by(relx::query::desc(u.age));

  EXPECT_EQ(query.to_sql(),
            "SELECT DISTINCT users.name, users.age FROM users ORDER BY users.age DESC");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithLimitOffset) {
  users u;
  auto query = relx::query::select_distinct(u.name, u.age).from(u).limit(10).offset(5);

  EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.name, users.age FROM users LIMIT ? OFFSET ?");

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "10");
  EXPECT_EQ(params[1], "5");
}

TEST(DistinctSelectTest, SelectDistinctAllColumns) {
  // Test SELECT DISTINCT *
  users u;
  auto query = relx::query::select_distinct_all(u);

  EXPECT_EQ(query.to_sql(), "SELECT DISTINCT * FROM users");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctAllColumnsWithTemplateArg) {
  // Test SELECT DISTINCT * using template argument
  auto query = relx::query::select_distinct_all<users>();

  EXPECT_EQ(query.to_sql(), "SELECT DISTINCT * FROM users");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctExpressions) {
  // Test SELECT DISTINCT with expressions
  users u;

  auto query = relx::query::select_distinct_expr(relx::query::as(u.id, "user_id"),
                                                 relx::query::as(u.name, "user_name"))
                   .from(u);

  EXPECT_EQ(query.to_sql(),
            "SELECT DISTINCT users.id AS user_id, users.name AS user_name FROM users");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithMixedExpressions) {
  // Test SELECT DISTINCT with a mix of column references and expressions
  users u;

  auto query = relx::query::select_distinct(u.id, relx::query::val(42),
                                            relx::query::as(u.name, "user_name"))
                   .from(u);

  EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.id, ?, users.name AS user_name FROM users");

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "42");
}

TEST(DistinctSelectTest, ComparisonWithDistinctExpr) {
  // Compare using select_distinct with using distinct() function from before
  users u;

  // New API way
  auto query1 = relx::query::select_distinct(u.age).from(u);

  // Old way using distinct() expression
  auto query2 = relx::query::select_expr(relx::query::distinct(u.age)).from(u);

  // Both should generate the same SQL
  EXPECT_EQ(query1.to_sql(), "SELECT DISTINCT users.age FROM users");
}