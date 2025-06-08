#include "test_common.hpp"

#include <gtest/gtest.h>

using namespace test_tables;
using namespace test_utils;

TEST(BasicSelectTest, SimpleSelect) {
  users u;

  auto query = relx::query::select(u.id, u.name, u.email).from(u);

  std::string expected_sql = "SELECT users.id, users.name, users.email FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectAllColumns) {
  users u;
  auto query = relx::query::select(u.id, u.name, u.email, u.age, u.created_at, u.is_active, u.bio,
                                   u.login_count)
                   .from(u);

  std::string expected_sql =
      "SELECT users.id, users.name, users.email, users.age, users.created_at, users.is_active, "
      "users.bio, users.login_count FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithExplicitTableName) {
  users u;
  posts p;
  auto query = relx::query::select(u.id, p.id).from(u).from(p);

  std::string expected_sql = "SELECT users.id, posts.id FROM users, posts";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithColumnAliases) {
  users u;

  auto query = relx::query::select_expr(relx::query::as(u.id, "user_id"),
                                        relx::query::as(u.name, "user_name"),
                                        relx::query::as(u.email, "user_email"))
                   .from(u);

  std::string expected_sql =
      "SELECT users.id AS user_id, users.name AS user_name, users.email AS user_email FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLiteral) {
  users u;

  auto query = relx::query::select_expr(u.id, relx::query::val(42),
                                        relx::query::val("constant string"))
                   .from(u);

  std::string expected_sql = "SELECT users.id, ?, ? FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "42");
  EXPECT_EQ(params[1], "constant string");
}

TEST(BasicSelectTest, SelectWithDistinct) {
  users u;

  auto query = relx::query::select_expr(relx::query::distinct(u.age)).from(u);

  std::string expected_sql = "SELECT DISTINCT users.age FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLimit) {
  users u;

  auto query = relx::query::select(u.id, u.name).from(u).limit(10);

  std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "10");
}

TEST(BasicSelectTest, SelectWithLimitAndOffset) {
  users u;

  auto query = relx::query::select(u.id, u.name).from(u).limit(10).offset(20);

  std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ? OFFSET ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "10");
  EXPECT_EQ(params[1], "20");
}

TEST(BasicSelectTest, SelectWithOrderByAsc) {
  users u;

  auto query = relx::query::select(u.id, u.name).from(u).order_by(relx::query::asc(u.name));

  std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.name ASC";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithOrderByDesc) {
  users u;

  auto query = relx::query::select(u.id, u.name).from(u).order_by(relx::query::desc(u.age));

  std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.age DESC";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithMultipleOrderBy) {
  users u;

  auto query = relx::query::select(u.id, u.name)
                   .from(u)
                   .order_by(relx::query::desc(u.age), relx::query::asc(u.name));

  std::string expected_sql =
      "SELECT users.id, users.name FROM users ORDER BY users.age DESC, users.name ASC";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLimitNew) {
  users u;
  auto query = relx::query::select(u.id, u.name).from(u).limit(10);

  std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "10");
}

TEST(BasicSelectTest, SelectWithLimitAndOffsetNew) {
  users u;
  auto query = relx::query::select(u.id, u.name).from(u).limit(10).offset(20);

  std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ? OFFSET ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "10");
  EXPECT_EQ(params[1], "20");
}

TEST(BasicSelectTest, SelectWithOrderByAscNew) {
  users u;
  auto query = relx::query::select(u.id, u.name).from(u).order_by(relx::query::asc(u.name));

  std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.name ASC";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithOrderByDescNew) {
  users u;
  auto query = relx::query::select(u.id, u.name).from(u).order_by(relx::query::desc(u.age));

  std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.age DESC";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithMultipleOrderByNew) {
  users u;
  auto query = relx::query::select(u.id, u.name)
                   .from(u)
                   .order_by(relx::query::desc(u.age), relx::query::asc(u.name));

  std::string expected_sql =
      "SELECT users.id, users.name FROM users ORDER BY users.age DESC, users.name ASC";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithConditionNew) {
  users u;
  auto query = relx::query::select(u.id, u.name).from(u).where(u.age > 18);

  std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.age > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "18");
}

TEST(BasicSelectTest, SelectWithMultipleConditionsNew) {
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

TEST(BasicSelectTest, SelectWithJoinNew) {
  users u;
  posts p;
  auto query =
      relx::query::select(u.name, p.title).from(u).join(p, relx::query::on(u.id == p.user_id));

  std::string expected_sql =
      "SELECT users.name, posts.title FROM users JOIN posts ON (users.id = posts.user_id)";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectFromHelper) {
  users u;
  auto query = relx::query::select(u.id, u.name, u.email).from(u);

  std::string expected_sql = "SELECT users.id, users.name, users.email FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());

  auto query_with_where = query.where(u.age > 18);
  std::string expected_where_sql =
      "SELECT users.id, users.name, users.email FROM users WHERE (users.age > ?)";
  EXPECT_EQ(query_with_where.to_sql(), expected_where_sql);

  auto params = query_with_where.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "18");
}
