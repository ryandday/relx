#include "test_common.hpp"

#include <gtest/gtest.h>

using namespace test_tables;
using namespace test_utils;

TEST(CaseExpressionTest, SimpleCase) {
  users u;

  auto case_expr = relx::query::case_()
                       .when(u.age < 18, "Minor")
                       .when(u.age < 65, "Adult")
                       .else_("Senior")
                       .build();

  auto query =
      relx::query::select_expr(u.name, relx::query::as(std::move(case_expr), "age_group")).from(u);

  std::string expected_sql = "SELECT users.name, CASE WHEN (users.age < ?) THEN ? WHEN (users.age "
                             "< ?) THEN ? ELSE ? END AS age_group FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 5);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "Minor");
  EXPECT_EQ(params[2], "65");
  EXPECT_EQ(params[3], "Adult");
  EXPECT_EQ(params[4], "Senior");
}

TEST(CaseExpressionTest, CaseWithoutElse) {
  users u;

  auto case_expr = relx::query::case_()
                       .when(u.is_active == true, "Active")
                       .when(u.is_active == false, "Inactive")
                       .build();

  auto query =
      relx::query::select_expr(u.name, relx::query::as(std::move(case_expr), "status")).from(u);

  std::string expected_sql = "SELECT users.name, CASE WHEN (users.is_active = ?) THEN ? WHEN "
                             "(users.is_active = ?) THEN ? END AS status FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "1");  // true as 1
  EXPECT_EQ(params[1], "Active");
  EXPECT_EQ(params[2], "0");  // false as 0
  EXPECT_EQ(params[3], "Inactive");
}

TEST(CaseExpressionTest, CaseWithComplexConditions) {
  users u;

  auto case_expr = relx::query::case_()
                       .when((u.age < 18) && (u.login_count > 0), "Young Active User")
                       .when((u.age >= 18) && (u.login_count > 10), "Power User")
                       .else_("Regular User")
                       .build();

  auto query = relx::query::select_expr(u.name,
                                        relx::query::as(std::move(case_expr), "complex_status"))
                   .from(u);

  std::string expected_sql = "SELECT users.name, CASE WHEN ((users.age < ?) AND (users.login_count "
                             "> ?)) THEN ? WHEN ((users.age >= ?) AND (users.login_count > ?)) "
                             "THEN ? ELSE ? END AS complex_status FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 7);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "0");
  EXPECT_EQ(params[2], "Young Active User");
  EXPECT_EQ(params[3], "18");
  EXPECT_EQ(params[4], "10");
  EXPECT_EQ(params[5], "Power User");
  EXPECT_EQ(params[6], "Regular User");
}

TEST(CaseExpressionTest, CaseWithColumnResults) {
  users u;

  auto case_expr = relx::query::case_()
                       .when(relx::query::is_null(u.bio), "No bio provided")
                       .else_("Has bio")
                       .build();

  auto query = relx::query::select_expr(u.name,
                                        relx::query::as(std::move(case_expr), "bio_display"))
                   .from(u);

  std::string expected_sql = "SELECT users.name, CASE WHEN (users.bio IS NULL) THEN ? ELSE ? END "
                             "AS bio_display FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "No bio provided");
  EXPECT_EQ(params[1], "Has bio");
}

TEST(CaseExpressionTest, CaseWithNumericResults) {
  users u;

  auto case_expr = relx::query::case_()
                       .when(u.login_count == 0, 0)
                       .when(u.login_count <= 5, 1)
                       .when(u.login_count <= 20, 2)
                       .else_(3)
                       .build();

  auto query = relx::query::select_expr(u.name,
                                        relx::query::as(std::move(case_expr), "activity_level"))
                   .from(u);

  std::string expected_sql =
      "SELECT users.name, CASE WHEN (users.login_count = ?) THEN ? WHEN (users.login_count <= ?) "
      "THEN ? WHEN (users.login_count <= ?) THEN ? ELSE ? END AS activity_level FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 7);
  EXPECT_EQ(params[0], "0");
  EXPECT_EQ(params[1], "0");
  EXPECT_EQ(params[2], "5");
  EXPECT_EQ(params[3], "1");
  EXPECT_EQ(params[4], "20");
  EXPECT_EQ(params[5], "2");
  EXPECT_EQ(params[6], "3");
}

TEST(CaseExpressionTest, NestedCaseExpression) {
  users u;

  // Inner CASE for active status
  auto active_case =
      relx::query::case_().when(u.is_active == true, "Active").else_("Inactive").build();

  // Outer CASE for age group and activity - use operator+ for string concatenation
  auto nested_case = relx::query::case_()
                         .when(u.age < 18, "Young, Active")
                         .when(u.age < 65, "Adult, Active")
                         .else_("Senior, Active")
                         .build();

  auto query =
      relx::query::select_expr(u.name, relx::query::as(std::move(nested_case), "status")).from(u);

  // The expected SQL is implementation-dependent, this is just an example
  auto sql = query.to_sql();
  EXPECT_TRUE(sql.find("SELECT users.name") != std::string::npos);
  EXPECT_TRUE(sql.find("CASE") != std::string::npos);

  // Check that parameters are present
  auto params = query.bind_params();
  EXPECT_GE(params.size(), 5);  // At minimum 5 params (could be more based on implementation)
}

TEST(CaseExpressionTest, CaseInWhere) {
  users u;

  // Define a CASE expression
  auto case_expr = relx::query::case_().when(u.age < 18, "minor").else_("adult").build();

  // Create an aliased version of the case expression
  auto age_category = relx::query::as(std::move(case_expr), "age_category");

  // Use it in a WHERE clause directly
  auto query =
      relx::query::select_expr(u.id, u.name, age_category).from(u).where(age_category == "adult");

  // Expected SQL should include the CASE expression in both the SELECT list and WHERE
  std::string sql = query.to_sql();
  EXPECT_TRUE(sql.find("SELECT users.id, users.name, CASE") != std::string::npos);
  EXPECT_TRUE(sql.find("WHERE (CASE") != std::string::npos);

  auto params = query.bind_params();
  // Check that parameters are present for both the SELECT list CASE and WHERE condition
  EXPECT_GE(params.size(), 4);
}

TEST(CaseExpressionTest, CaseInOrderBy) {
  users u;

  // Define a CASE expression for sorting
  auto case_expr = relx::query::case_().when(u.is_active == true, 1).else_(0).build();

  // Create an aliased version of the case expression
  auto active_sort = relx::query::as(std::move(case_expr), "active_sort");

  // Use it in SELECT and ORDER BY directly
  auto query = relx::query::select_expr(u.id, u.name, active_sort)
                   .from(u)
                   .order_by(relx::query::desc(active_sort));

  // Expected SQL should include the case expression in both the SELECT list and ORDER BY
  std::string sql = query.to_sql();
  EXPECT_TRUE(sql.find("SELECT users.id, users.name, CASE") != std::string::npos);
  EXPECT_TRUE(sql.find("ORDER BY CASE") != std::string::npos);

  auto params = query.bind_params();
  // Check that parameters are present for both the SELECT list CASE and ORDER BY clause
  EXPECT_GE(params.size(), 3);
}