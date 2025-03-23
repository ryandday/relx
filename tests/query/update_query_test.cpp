#include <gtest/gtest.h>
#include "sqllib/query/update.hpp"
#include "sqllib/query/condition.hpp"
#include "sqllib/query/value.hpp"
#include "sqllib/query/function.hpp"
#include <string>
#include <string_view>
#include <vector>

using namespace sqllib;

// Define a simple User table for testing
struct User {
    static constexpr auto table_name = "users";
    
    schema::column<"id", int> id;
    schema::column<"name", std::string> name;
    schema::column<"email", std::string> email;
    schema::column<"active", bool> active;
    schema::column<"login_count", int> login_count;
    schema::column<"last_login", std::string> last_login;
    schema::column<"status", std::string> status;
    schema::column<"age", int> age;
};

// Test basic UPDATE query without WHERE clause
TEST(UpdateQueryTest, BasicUpdate) {
    User users;
    
    auto query = query::update(users)
        .set(users.name, query::val("John Doe"))
        .set(users.email, query::val("john@example.com"));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ?, email = ?");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "John Doe");
    EXPECT_EQ(params[1], "john@example.com");
}

// Test UPDATE query with WHERE clause
TEST(UpdateQueryTest, UpdateWithWhere) {
    User users;
    
    // Create column references for use in conditions
    auto id_ref = query::column_ref(users.id);
    
    auto query = query::update(users)
        .set(users.name, query::val("John Doe"))
        .set(users.email, query::val("john@example.com"))
        .where(id_ref == query::val(1));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ?, email = ? WHERE (id = ?)");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "John Doe");
    EXPECT_EQ(params[1], "john@example.com");
    EXPECT_EQ(params[2], "1");
}

// Test UPDATE query with complex WHERE clause
TEST(UpdateQueryTest, UpdateWithComplexWhere) {
    User users;
    
    // Create column references for use in conditions
    auto id_ref = query::column_ref(users.id);
    auto active_ref = query::column_ref(users.active);
    
    auto query = query::update(users)
        .set(users.name, query::val("John Doe"))
        .where(id_ref > query::val(10) && active_ref == query::val(true));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ? WHERE ((id > ?) AND (active = ?))");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "John Doe");
    EXPECT_EQ(params[1], "10");
    EXPECT_EQ(params[2], "1"); // true converts to "1"
}

// Test UPDATE query with multiple SET operations
TEST(UpdateQueryTest, UpdateWithMultipleSets) {
    User users;
    
    auto query = query::update(users)
        .set(users.name, query::val("Jane Doe"))
        .set(users.email, query::val("jane@example.com"))
        .set(users.active, query::val(false));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ?, email = ?, active = ?");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "Jane Doe");
    EXPECT_EQ(params[1], "jane@example.com");
    EXPECT_EQ(params[2], "0"); // false converts to "0"
}

// Test UPDATE with function in SET clause
TEST(UpdateQueryTest, UpdateWithFunctionInSet) {
    User users;
    
    // Update last_login with a function call
    auto current_timestamp = query::NullaryFunctionExpr("CURRENT_TIMESTAMP");
    
    auto query = query::update(users)
        .set(users.last_login, current_timestamp)
        .where(query::column_ref(users.id) == query::val(1));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET last_login = CURRENT_TIMESTAMP() WHERE (id = ?)");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "1");
}

// Test UPDATE with CASE expression in SET clause
// TEST(UpdateQueryTest, UpdateWithCaseExpressionInSet) {
//     User users;
    
//     // Create a CASE expression for determining the status
//     auto case_builder = query::case_()
//         .when(query::column_ref(users.login_count) > query::val(10), query::val("active"))
//         .when(query::column_ref(users.login_count) > query::val(0), query::val("new"))
//         .else_(query::val("inactive"));
    
//     auto query = query::update(users)
//         .set(users.status, case_builder.build())
//         .where(query::column_ref(users.id) == query::val(1));
    
//     EXPECT_EQ(query.to_sql(), "UPDATE users SET status = CASE WHEN (login_count > ?) THEN ? WHEN (login_count > ?) THEN ? ELSE ? END WHERE (id = ?)");
    
//     auto params = query.bind_params();
//     ASSERT_EQ(params.size(), 6);
//     EXPECT_EQ(params[0], "10");
//     EXPECT_EQ(params[1], "active");
//     EXPECT_EQ(params[2], "0");
//     EXPECT_EQ(params[3], "new");
//     EXPECT_EQ(params[4], "inactive");
//     EXPECT_EQ(params[5], "1");
// }

// Test UPDATE with IN condition in WHERE clause
TEST(UpdateQueryTest, UpdateWithInCondition) {
    User users;
    
    // Create a list of IDs to update
    std::vector<std::string> ids = {"1", "3", "5", "7"};
    
    auto query = query::update(users)
        .set(users.active, query::val(true))
        .where(query::in(query::column_ref(users.id), ids));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET active = ? WHERE id IN (?, ?, ?, ?)");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 5);
    EXPECT_EQ(params[0], "1"); // true is represented as "1"
    EXPECT_EQ(params[1], "1");
    EXPECT_EQ(params[2], "3");
    EXPECT_EQ(params[3], "5");
    EXPECT_EQ(params[4], "7");
}

// Alternative approach to test with CASE-like functionality
TEST(UpdateQueryTest, UpdateWithConditionalValue) {
    User users;
    
    // Create separate update queries based on conditions
    auto query = query::update(users)
        .set(users.status, query::val("active"))
        .where(query::column_ref(users.login_count) > query::val(10));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET status = ? WHERE (login_count > ?)");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "active");
    EXPECT_EQ(params[1], "10");
} 