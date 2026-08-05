#include "relx/query/function.hpp"
#include "relx/query/insert.hpp"
#include "relx/query/select.hpp"
#include "relx/query/value.hpp"

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <relx/schema.hpp>

using namespace relx;

// Define a simple User table for testing
// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

struct [[=relx::table("users")]] User {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string email;
  bool active;
  int login_count;
  std::string last_login;
  std::string status;
  int age;
};
constexpr auto users = relx::t<User>;

// Define a Posts table for INSERT ... SELECT tests
struct [[=relx::table("posts")]] Post {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^User::id>]] int user_id;
  std::string title;
  std::string content;
  std::string created_at;
};
constexpr auto posts = relx::t<Post>;

// Test table struct
struct [[=relx::table("insert_test")]] InsertTestTable {
  [[=relx::ann::pk]] int id;
  std::string name;
  int age;
  bool active;
};
constexpr auto insert_test = relx::t<InsertTestTable>;

}  // namespace

// clang-format on

// Test basic INSERT with explicit columns and values
TEST(InsertQueryTest, BasicInsert) {
  auto query = query::insert_into(users)
                   .columns(users.name, users.email, users.active)
                   .values(query::val("John Doe"), query::val("john@example.com"),
                           query::val(true));

  EXPECT_EQ(query.to_sql(), "INSERT INTO users (name, email, active) VALUES (?, ?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "John Doe");
  EXPECT_EQ(params[1], "john@example.com");
  EXPECT_EQ(params[2], "true");
}

// Test INSERT with multiple rows
TEST(InsertQueryTest, InsertMultipleRows) {
  auto query = query::insert_into(users)
                   .columns(users.name, users.email)
                   .values(query::val("John Doe"), query::val("john@example.com"))
                   .values(query::val("Jane Smith"), query::val("jane@example.com"));

  EXPECT_EQ(query.to_sql(), "INSERT INTO users (name, email) VALUES (?, ?), (?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "John Doe");
  EXPECT_EQ(params[1], "john@example.com");
  EXPECT_EQ(params[2], "Jane Smith");
  EXPECT_EQ(params[3], "jane@example.com");
}

// Test INSERT using expression instead of literal values
TEST(InsertQueryTest, InsertWithExpressions) {
  // Use a function expression
  auto current_timestamp = query::NullaryFunctionExpr("CURRENT_TIMESTAMP");

  auto query = query::insert_into(users)
                   .columns(users.name, users.email, users.last_login)
                   .values(query::val("John Doe"), query::val("john@example.com"),
                           current_timestamp);

  EXPECT_EQ(query.to_sql(),
            "INSERT INTO users (name, email, last_login) VALUES (?, ?, CURRENT_TIMESTAMP())");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "John Doe");
  EXPECT_EQ(params[1], "john@example.com");
}

// Test INSERT ... SELECT
TEST(InsertQueryTest, InsertWithSelect) {
  auto select_query = query::select(users.id, users.name, query::val("default@example.com"))
                          .from(users)
                          .where(query::column_ref(users.active) == query::val(true));

  auto query = query::insert_into(posts)
                   .columns(posts.user_id, posts.title, posts.content)
                   .select(select_query);

  EXPECT_EQ(query.to_sql(), "INSERT INTO posts (user_id, title, content) SELECT users.id, "
                            "users.name, ? FROM users WHERE (users.active = ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "default@example.com");
  EXPECT_EQ(params[1], "true");
}

// Test INSERT with multiple rows of mixed literal and expression values
TEST(InsertQueryTest, InsertMultipleRowsWithMixedValues) {
  // Use a function expression
  auto current_timestamp = query::NullaryFunctionExpr("CURRENT_TIMESTAMP");

  auto query = query::insert_into(users)
                   .columns(users.name, users.email, users.last_login, users.active)
                   .values(query::val("John Doe"), query::val("john@example.com"),
                           current_timestamp, query::val(true))
                   .values(query::val("Jane Smith"), query::val("jane@example.com"),
                           current_timestamp, query::val(false));

  EXPECT_EQ(query.to_sql(), "INSERT INTO users (name, email, last_login, active) VALUES (?, ?, "
                            "CURRENT_TIMESTAMP(), ?), (?, ?, CURRENT_TIMESTAMP(), ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 6);
  EXPECT_EQ(params[0], "John Doe");
  EXPECT_EQ(params[1], "john@example.com");
  EXPECT_EQ(params[2], "true");
  EXPECT_EQ(params[3], "Jane Smith");
  EXPECT_EQ(params[4], "jane@example.com");
  EXPECT_EQ(params[5], "false");
}

// Test error handling: INSERT without specifying columns
TEST(InsertQueryTest, InsertWithoutColumns) {
  // This is valid SQL but might not be what the user intends
  // The library should support it but we might want to warn about it in documentation
  auto query = query::insert_into(users).values(query::val(1), query::val("John Doe"),
                                                query::val("john@example.com"));

  EXPECT_EQ(query.to_sql(), "INSERT INTO users VALUES (?, ?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "1");
  EXPECT_EQ(params[1], "John Doe");
  EXPECT_EQ(params[2], "john@example.com");
}

TEST(InsertQueryTest, InsertWithRawValues) {
  // Test inserting with raw values (not wrapped in val())
  auto query = relx::query::insert_into(insert_test)
                   .columns(insert_test.name, insert_test.age, insert_test.active)
                   .values("John Doe", 30, true);

  std::string expected_sql = "INSERT INTO insert_test (name, age, active) VALUES (?, ?, ?)";
  EXPECT_EQ(expected_sql, query.to_sql());

  auto params = query.bind_params();
  ASSERT_EQ(3, params.size());
  EXPECT_EQ("John Doe", params[0]);
  EXPECT_EQ("30", params[1]);
  EXPECT_EQ("true", params[2]);

  // Test with multiple rows of raw values
  auto multi_query = relx::query::insert_into(insert_test)
                         .columns(insert_test.name, insert_test.age, insert_test.active)
                         .values("John Doe", 30, true)
                         .values("Jane Smith", 25, false);

  std::string expected_multi_sql =
      "INSERT INTO insert_test (name, age, active) VALUES (?, ?, ?), (?, ?, ?)";
  EXPECT_EQ(expected_multi_sql, multi_query.to_sql());

  auto multi_params = multi_query.bind_params();
  ASSERT_EQ(6, multi_params.size());
  EXPECT_EQ("John Doe", multi_params[0]);
  EXPECT_EQ("30", multi_params[1]);
  EXPECT_EQ("true", multi_params[2]);
  EXPECT_EQ("Jane Smith", multi_params[3]);
  EXPECT_EQ("25", multi_params[4]);
  EXPECT_EQ("false", multi_params[5]);
}

// Test INSERT with RETURNING clause
TEST(InsertQueryTest, InsertWithReturning) {
  // Test basic returning with column references
  auto basic_query = query::insert_into(users)
                         .columns(users.name, users.email, users.active)
                         .values("John Doe", "john@example.com", true)
                         .returning(query::column_ref(users.id), query::column_ref(users.name));

  EXPECT_EQ(
      basic_query.to_sql(),
      "INSERT INTO users (name, email, active) VALUES (?, ?, ?) RETURNING users.id, users.name");

  auto basic_params = basic_query.bind_params();
  ASSERT_EQ(basic_params.size(), 3);
  EXPECT_EQ(basic_params[0], "John Doe");
  EXPECT_EQ(basic_params[1], "john@example.com");
  EXPECT_EQ(basic_params[2], "true");

  // Test basic returning with direct column references
  auto direct_column_query = query::insert_into(users)
                                 .columns(users.name, users.email, users.active)
                                 .values("John Doe", "john@example.com", true)
                                 .returning(users.id, users.name);

  EXPECT_EQ(
      direct_column_query.to_sql(),
      "INSERT INTO users (name, email, active) VALUES (?, ?, ?) RETURNING users.id, users.name");

  auto direct_params = direct_column_query.bind_params();
  ASSERT_EQ(direct_params.size(), 3);
  EXPECT_EQ(direct_params[0], "John Doe");
  EXPECT_EQ(direct_params[1], "john@example.com");
  EXPECT_EQ(direct_params[2], "true");

  // Test returning with expressions
  auto count_func = query::NullaryFunctionExpr("COUNT");
  auto expr_query = query::insert_into(users)
                        .columns(users.name, users.email)
                        .values("Jane Smith", "jane@example.com")
                        .returning(query::column_ref(users.id), count_func,
                                   query::as(users.name, "inserted_name"));

  EXPECT_EQ(expr_query.to_sql(), "INSERT INTO users (name, email) VALUES (?, ?) RETURNING "
                                 "users.id, COUNT(), users.name AS inserted_name");

  auto expr_params = expr_query.bind_params();
  ASSERT_EQ(expr_params.size(), 2);
  EXPECT_EQ(expr_params[0], "Jane Smith");
  EXPECT_EQ(expr_params[1], "jane@example.com");

  // Test mixed direct columns and expressions
  auto mixed_query = query::insert_into(users)
                         .columns(users.name, users.email)
                         .values("Jane Smith", "jane@example.com")
                         .returning(users.id,    // Direct column reference
                                    count_func,  // SQL expression
                                    query::as(users.name, "inserted_name")  // Aliased column
                         );

  EXPECT_EQ(mixed_query.to_sql(), "INSERT INTO users (name, email) VALUES (?, ?) RETURNING "
                                  "users.id, COUNT(), users.name AS inserted_name");

  auto mixed_params = mixed_query.bind_params();
  ASSERT_EQ(mixed_params.size(), 2);
  EXPECT_EQ(mixed_params[0], "Jane Smith");
  EXPECT_EQ(mixed_params[1], "jane@example.com");

  // Test returning with INSERT ... SELECT
  auto select_query = query::select(users.id, users.name, query::val("default@example.com"))
                          .from(users)
                          .where(query::column_ref(users.active) == query::val(true));

  auto select_insert_query = query::insert_into(users)
                                 .columns(users.id, users.name, users.email)
                                 .select(select_query)
                                 .returning(users.id);  // Direct column reference

  EXPECT_EQ(select_insert_query.to_sql(),
            "INSERT INTO users (id, name, email) SELECT users.id, users.name, ? FROM users WHERE "
            "(users.active = ?) RETURNING users.id");

  auto select_params = select_insert_query.bind_params();
  ASSERT_EQ(select_params.size(), 2);
  EXPECT_EQ(select_params[0], "default@example.com");
  EXPECT_EQ(select_params[1], "true");
}