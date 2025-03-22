#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(BasicSelectTest, SimpleSelect) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name, u.email)
        .from(u);
    
    std::string expected_sql = "SELECT id, name, email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectAllColumns) {
    users u;
    
    // Using an asterisk-like approach with all columns
    auto query = sqllib::query::select(u.id, u.name, u.email, u.age, u.created_at, u.is_active, u.bio, u.login_count)
        .from(u);
    
    std::string expected_sql = "SELECT id, name, email, age, created_at, is_active, bio, login_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithExplicitTableName) {
    users u;
    posts p;
    
    // When selecting from multiple tables, column names should use table prefixes
    auto query = sqllib::query::select_expr(
        sqllib::query::to_expr(u.id),
        sqllib::query::to_expr(p.id)
    )
    .from(u)
    .from(p);
    
    std::string expected_sql = "SELECT id, id FROM users, posts";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithColumnAliases) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::to_expr(u.id), "user_id"),
        sqllib::query::as(sqllib::query::to_expr(u.name), "user_name"),
        sqllib::query::as(sqllib::query::to_expr(u.email), "user_email")
    )
    .from(u);
    
    std::string expected_sql = "SELECT id AS user_id, name AS user_name, email AS user_email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLiteral) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::to_expr(u.id),
        sqllib::query::val(42),
        sqllib::query::val("constant string")
    )
    .from(u);
    
    std::string expected_sql = "SELECT id, ?, ? FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "42");
    EXPECT_EQ(params[1], "constant string");
}

TEST(BasicSelectTest, SelectWithDistinct) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::distinct(sqllib::query::to_expr(u.age))
    )
    .from(u);
    
    std::string expected_sql = "SELECT DISTINCT age FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLimit) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .limit(10);
    
    std::string expected_sql = "SELECT id, name FROM users LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "10");
}

TEST(BasicSelectTest, SelectWithLimitAndOffset) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .limit(10)
        .offset(20);
    
    std::string expected_sql = "SELECT id, name FROM users LIMIT ? OFFSET ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "10");
    EXPECT_EQ(params[1], "20");
}

TEST(BasicSelectTest, SelectWithOrderByAsc) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .order_by(sqllib::query::asc(sqllib::query::to_expr(u.name)));
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithOrderByDesc) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .order_by(sqllib::query::desc(sqllib::query::to_expr(u.age)));
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY age DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithMultipleOrderBy) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .order_by(
            sqllib::query::desc(sqllib::query::to_expr(u.age)),
            sqllib::query::asc(sqllib::query::to_expr(u.name))
        );
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY age DESC, name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
} 