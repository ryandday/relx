#include <gtest/gtest.h>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

// Define test tables
struct users {
    static constexpr auto table_name = "users";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"age", int> age;
    sqllib::schema::column<"created_at", std::string> created_at;
    sqllib::schema::column<"is_active", bool> is_active;
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
    sqllib::schema::column<"login_count", int> login_count;
    
    sqllib::schema::primary_key<&users::id> pk;
    sqllib::schema::unique_constraint<&users::email> unique_email;
};

// Define a second table
struct posts {
    static constexpr auto table_name = "posts";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    
    sqllib::schema::primary_key<&posts::id> pk;
    sqllib::schema::foreign_key<&posts::user_id, &users::id> user_fk;
};

TEST(SelectAllTest, BasicSelectAll) {
    users u;
    
    // Use select_all with a table instance
    auto query = sqllib::query::select_all(u);
    
    // The expected SQL should include all columns but not constraints
    std::string expected_sql = "SELECT * FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithoutInstance) {
    // Use select_all with just the table type
    auto query = sqllib::query::select_all<users>();
    
    // The expected SQL should include all columns but not constraints
    std::string expected_sql = "SELECT * FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithWhere) {
    // Use select_all with just the table type and add a WHERE clause
    auto query = sqllib::query::select_all<users>()
        .where(sqllib::query::to_expr<&users::age>() > 18);
    
    // The expected SQL should include all columns with the WHERE clause
    std::string expected_sql = "SELECT * FROM users WHERE (age > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(SelectAllTest, SelectAllWithJoin) {
    // Use select_all with a join
    auto query = sqllib::query::select_all<users>()
        .join(posts{}, sqllib::query::on(sqllib::query::to_expr<&users::id>() == sqllib::query::to_expr<&posts::user_id>()));
    
    // The expected SQL should include all columns from users and the JOIN clause
    std::string expected_sql = "SELECT * FROM users JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithAllClauses) {
    // Use select_all with multiple clauses
    auto query = sqllib::query::select_all<users>()
        .join(posts{}, sqllib::query::on(sqllib::query::to_expr<&users::id>() == sqllib::query::to_expr<&posts::user_id>()))
        .where(sqllib::query::to_expr<&users::age>() > 18)
        .group_by(sqllib::query::to_expr<&users::id>())
        .having(sqllib::query::count(sqllib::query::to_expr<&posts::id>()) > 5)
        .order_by(sqllib::query::desc(sqllib::query::to_expr<&users::age>()))
        .limit(10)
        .offset(20);
    
    // The expected SQL should include all clauses
    std::string expected_sql = "SELECT * FROM users "
                           "JOIN posts ON (id = user_id) "
                           "WHERE (age > ?) "
                           "GROUP BY id "
                           "HAVING (COUNT(id) > ?) "
                           "ORDER BY age DESC "
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