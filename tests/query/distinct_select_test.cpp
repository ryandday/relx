#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

/// Test cases for the SELECT DISTINCT functionality


TEST(DistinctSelectTest, SimpleSelectDistinctLegacy) {
    users u;
    
    auto query = sqllib::query::select_distinct(u.id, u.name, u.email)
        .from(u);
    
    std::string expected_sql = "SELECT DISTINCT id, name, email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithCondition) {
    // Test DISTINCT with WHERE condition
    auto query = sqllib::query::select_distinct<&users::id, &users::name>()
        .from(users{})
        .where(sqllib::query::to_expr<&users::age>() > 18);
    
    std::string expected_sql = "SELECT DISTINCT id, name FROM users WHERE (age > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(DistinctSelectTest, SelectDistinctWithJoin) {
    // Test DISTINCT with JOIN
    users u;
    posts p;
    auto query = sqllib::query::select_distinct(u.id, p.title)
        .from(u)
        .join(p, sqllib::query::on(
            u.id == p.user_id
        ));
    
    std::string expected_sql = "SELECT DISTINCT id, title FROM users JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithGroupBy) {
    // Test DISTINCT with GROUP BY
    auto query = sqllib::query::select_distinct<&users::name, &users::age>()
        .from(users{})
        .group_by(sqllib::query::to_expr<&users::age>());
    
    std::string expected_sql = "SELECT DISTINCT name, age FROM users GROUP BY age";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithOrderBy) {
    // Test DISTINCT with ORDER BY
    auto query = sqllib::query::select_distinct<&users::name, &users::age>()
        .from(users{})
        .order_by(sqllib::query::desc(sqllib::query::to_expr<&users::age>()));
    
    std::string expected_sql = "SELECT DISTINCT name, age FROM users ORDER BY age DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithLimitOffset) {
    // Test DISTINCT with LIMIT and OFFSET
    auto query = sqllib::query::select_distinct<&users::name, &users::age>()
        .from(users{})
        .limit(10)
        .offset(5);
    
    std::string expected_sql = "SELECT DISTINCT name, age FROM users LIMIT ? OFFSET ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "10");
    EXPECT_EQ(params[1], "5");
}

TEST(DistinctSelectTest, SelectDistinctAllColumns) {
    // Test SELECT DISTINCT * 
    users u;
    auto query = sqllib::query::select_distinct_all(u);
    
    std::string expected_sql = "SELECT DISTINCT * FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctAllColumnsWithTemplateArg) {
    // Test SELECT DISTINCT * using template argument
    auto query = sqllib::query::select_distinct_all<users>();
    
    std::string expected_sql = "SELECT DISTINCT * FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctExpressions) {
    // Test SELECT DISTINCT with expressions
    users u;
    
    auto query = sqllib::query::select_distinct_expr(
        sqllib::query::as(u.id, "user_id"),
        sqllib::query::as(u.name, "user_name")
    )
    .from(u);
    
    std::string expected_sql = "SELECT DISTINCT id AS user_id, name AS user_name FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithMixedExpressions) {
    // Test SELECT DISTINCT with a mix of column references and expressions
    users u;
    
    auto query = sqllib::query::select_distinct(
        u.id,
        sqllib::query::val(42),
        sqllib::query::as(u.name, "user_name")
    )
    .from(u);
    
    std::string expected_sql = "SELECT DISTINCT id, ?, name AS user_name FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "42");
}

TEST(DistinctSelectTest, ComparisonWithDistinctExpr) {
    // Compare using select_distinct with using distinct() function from before
    users u;
    
    // New API way
    auto query1 = sqllib::query::select_distinct(u.age)
        .from(u);
    
    // Old way using distinct() expression
    auto query2 = sqllib::query::select_expr(
        sqllib::query::distinct(u.age)
    )
    .from(u);
    
    // Both should generate the same SQL
    EXPECT_EQ(query1.to_sql(), query2.to_sql());
    EXPECT_EQ(query1.to_sql(), "SELECT DISTINCT age FROM users");
} 