#include <gtest/gtest.h>
#include "relx/query/update.hpp"
#include "relx/query/condition.hpp"
#include "relx/query/value.hpp"
#include "relx/query/function.hpp"
#include <string>
#include <string_view>
#include <vector>

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

// Test basic UPDATE query without WHERE clause
TEST(UpdateQueryTest, BasicUpdate) {
    User users;
    
    auto query = query::update(users)
        .set(users.name, "John Doe")
        .set(users.email, "john@example.com");
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ?, email = ?");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "John Doe");
    EXPECT_EQ(params[1], "john@example.com");
}

// Test UPDATE query with WHERE clause
TEST(UpdateQueryTest, UpdateWithWhere) {
    User users;
    
    auto query = query::update(users)
        .set(users.name, "John Doe")
        .set(users.email, "john@example.com")
        .where(users.id == 1);
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ?, email = ? WHERE (users.id = ?)");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "John Doe");
    EXPECT_EQ(params[1], "john@example.com");
    EXPECT_EQ(params[2], "1");
}

// Test UPDATE query with complex WHERE clause
TEST(UpdateQueryTest, UpdateWithComplexWhere) {
    User users;
    
    auto query = query::update(users)
        .set(users.name, "John Doe")
        .where(users.id > 10 && users.active == true);
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET name = ? WHERE ((users.id > ?) AND (users.active = ?))");
    
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
        .set(users.name, "Jane Doe")
        .set(users.email, "jane@example.com")
        .set(users.active, false);
    
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
        .where(users.id == 1);
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET last_login = CURRENT_TIMESTAMP() WHERE (users.id = ?)");
    
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
        .set(users.active, true)
        .where(query::in(users.id, ids));
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET active = ? WHERE users.id IN (?, ?, ?, ?)");
    
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
    
    EXPECT_EQ(query.to_sql(), "UPDATE users SET status = ? WHERE (users.login_count > ?)");
    
    auto params = query.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "active");
    EXPECT_EQ(params[1], "10");
}

// Test UPDATE with RETURNING clause
TEST(UpdateQueryTest, UpdateWithReturning) {
    User users;
    
    // Test basic returning with column references
    auto basic_query = query::update(users)
        .set(users.name, "John Doe")
        .set(users.email, "john@example.com")
        .where(users.id == 1)
        .returning(query::column_ref(users.id), query::column_ref(users.name));
    
    EXPECT_EQ(basic_query.to_sql(), 
        "UPDATE users SET name = ?, email = ? WHERE (users.id = ?) RETURNING users.id, users.name");
    
    auto basic_params = basic_query.bind_params();
    ASSERT_EQ(basic_params.size(), 3);
    EXPECT_EQ(basic_params[0], "John Doe");
    EXPECT_EQ(basic_params[1], "john@example.com");
    EXPECT_EQ(basic_params[2], "1");
    
    // Test basic returning with direct column references
    auto direct_column_query = query::update(users)
        .set(users.name, "John Doe")
        .set(users.active, true)
        .where(users.id == 1)
        .returning(users.id, users.name);
    
    EXPECT_EQ(direct_column_query.to_sql(), 
        "UPDATE users SET name = ?, active = ? WHERE (users.id = ?) RETURNING users.id, users.name");
    
    auto direct_params = direct_column_query.bind_params();
    ASSERT_EQ(direct_params.size(), 3);
    EXPECT_EQ(direct_params[0], "John Doe");
    EXPECT_EQ(direct_params[1], "1"); // true becomes "1"
    EXPECT_EQ(direct_params[2], "1");
    
    // Test returning with expressions
    auto count_func = query::NullaryFunctionExpr("COUNT");
    auto expr_query = query::update(users)
        .set(users.name, "Jane Smith")
        .set(users.email, "jane@example.com")
        .where(users.active == true)
        .returning(
            query::column_ref(users.id), 
            count_func, 
            query::as(users.name, "updated_name")
        );
    
    EXPECT_EQ(expr_query.to_sql(), 
        "UPDATE users SET name = ?, email = ? WHERE (users.active = ?) RETURNING users.id, COUNT(), users.name AS updated_name");
    
    auto expr_params = expr_query.bind_params();
    ASSERT_EQ(expr_params.size(), 3);
    EXPECT_EQ(expr_params[0], "Jane Smith");
    EXPECT_EQ(expr_params[1], "jane@example.com");
    EXPECT_EQ(expr_params[2], "1");
    
    // Test mixed direct columns and expressions
    auto mixed_query = query::update(users)
        .set(users.name, "Jane Smith")
        .set(users.email, "jane@example.com")
        .where(users.active == true)
        .returning(
            users.id,  // Direct column reference
            count_func, // SQL expression
            query::as(users.name, "updated_name") // Aliased column
        );
    
    EXPECT_EQ(mixed_query.to_sql(), 
        "UPDATE users SET name = ?, email = ? WHERE (users.active = ?) RETURNING users.id, COUNT(), users.name AS updated_name");
    
    auto mixed_params = mixed_query.bind_params();
    ASSERT_EQ(mixed_params.size(), 3);
    EXPECT_EQ(mixed_params[0], "Jane Smith");
    EXPECT_EQ(mixed_params[1], "jane@example.com");
    EXPECT_EQ(mixed_params[2], "1");
} 
