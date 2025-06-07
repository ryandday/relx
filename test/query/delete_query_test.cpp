#include "relx/query/condition.hpp"
#include "relx/query/delete.hpp"
#include "relx/query/function.hpp"
#include "relx/query/value.hpp"

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using namespace relx;

// Define a simple User table for testing
struct User {
  static constexpr auto table_name = "users";

  schema::column<User, "id", int> id;
  schema::column<User, "name", std::string> name;
  schema::column<User, "email", std::string> email;
  schema::column<User, "active", bool> active;
  schema::column<User, "login_count", int> login_count;
  schema::column<User, "last_login", std::string> last_login;
  schema::column<User, "status", std::string> status;
  schema::column<User, "age", int> age;
};

// Test basic DELETE query without WHERE clause
TEST(DeleteQueryTest, BasicDelete) {
  User users;

  auto query = query::delete_from(users);

  EXPECT_EQ(query.to_sql(), "DELETE FROM users");
  EXPECT_TRUE(query.bind_params().empty());
}

// Test DELETE query with WHERE clause
TEST(DeleteQueryTest, DeleteWithWhere) {
  User users;

  // Create column references for use in conditions
  auto id_ref = query::column_ref(users.id);

  auto query = query::delete_from(users).where(id_ref == query::val(1));

  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE (users.id = ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "1");
}

// Test DELETE query with complex WHERE clause
TEST(DeleteQueryTest, DeleteWithComplexWhere) {
  User users;

  // Create column references for use in conditions
  auto id_ref = query::column_ref(users.id);
  auto active_ref = query::column_ref(users.active);

  auto query = query::delete_from(users).where(id_ref > query::val(10) &&
                                               active_ref == query::val(true));

  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE ((users.id > ?) AND (users.active = ?))");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "10");
  EXPECT_EQ(params[1], "1");  // true converts to "1"
}

// Test direct column comparison with value
TEST(DeleteQueryTest, DeleteWithDirectColumnComparison) {
  User users;

  // Use direct column comparison with value
  auto query = query::delete_from(users).where(users.id == 1);

  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE (users.id = ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "1");

  // Test more complex condition with direct column comparisons
  auto complex_query = query::delete_from(users).where(users.id > 10 && users.active == true);

  EXPECT_EQ(complex_query.to_sql(),
            "DELETE FROM users WHERE ((users.id > ?) AND (users.active = ?))");

  auto complex_params = complex_query.bind_params();
  ASSERT_EQ(complex_params.size(), 2);
  EXPECT_EQ(complex_params[0], "10");
  EXPECT_EQ(complex_params[1], "1");  // true converts to "1"
}

// Test DELETE with IN condition in WHERE clause
TEST(DeleteQueryTest, DeleteWithInCondition) {
  User users;

  // Create a list of IDs to delete
  std::vector<std::string> ids = {"1", "3", "5", "7"};

  auto query = query::delete_from(users).where(query::in(query::column_ref(users.id), ids));

  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE users.id IN (?, ?, ?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "1");
  EXPECT_EQ(params[1], "3");
  EXPECT_EQ(params[2], "5");
  EXPECT_EQ(params[3], "7");
}

// Test the convenience method for WHERE IN queries
TEST(DeleteQueryTest, DeleteWithWhereInMethod) {
  User users;

  // Create a list of IDs to delete
  std::vector<std::string> statuses = {"inactive", "banned", "deleted"};

  auto query = query::delete_from(users).where_in(users.status, statuses);

  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE users.status IN (?, ?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "inactive");
  EXPECT_EQ(params[1], "banned");
  EXPECT_EQ(params[2], "deleted");
}

// Test DELETE with multiple condition types
TEST(DeleteQueryTest, DeleteWithMultipleConditionTypes) {
  User users;

  auto query = query::delete_from(users).where(
      query::column_ref(users.age) < query::val(18) ||
      query::like(query::column_ref(users.email), "%@test.com"));

  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE ((users.age < ?) OR users.email LIKE ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "%@test.com");
}

// Test error handling scenarios - deleting without a WHERE clause is a common mistake
TEST(DeleteQueryTest, DeleteWithoutWhereClauseSafety) {
  User users;

  // In a real application, you might want to have a safety mechanism
  // to prevent accidental deletion of all records.
  // Here we're just testing that the SQL is correctly generated.
  auto query = query::delete_from(users);

  EXPECT_EQ(query.to_sql(), "DELETE FROM users");
  EXPECT_TRUE(query.bind_params().empty());

  // This would be a safer approach in a real application
  // where the API forces you to explicitly specify that you want to delete all:
  auto safer_query = query::delete_from(users).where(query::val(true) ==
                                                     query::val(true));  // Always true condition

  EXPECT_EQ(safer_query.to_sql(), "DELETE FROM users WHERE (? = ?)");

  auto params = safer_query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "1");
  EXPECT_EQ(params[1], "1");
}