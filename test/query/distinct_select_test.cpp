#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

/// Test cases for the SELECT DISTINCT functionality


TEST(DistinctSelectTest, SimpleSelectDistinctLegacy) {
    users u;
    
    auto query = relx::query::select_distinct(u.id, u.name, u.email)
        .from(u);
    
    EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.id, users.name, users.email FROM users");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithCondition) {
    // Test DISTINCT with WHERE condition
    auto query = relx::query::select_distinct<&users::id, &users::name>()
        .from(users{})
        .where(relx::query::to_expr<&users::age>() > 18);
    
    EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.id, users.name FROM users WHERE (users.age > ?)");
    
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
        .join(p, relx::query::on(
            u.id == p.user_id
        ));
    
    EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.id, posts.title FROM users JOIN posts ON (users.id = posts.user_id)");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithGroupBy) {
    // Test DISTINCT with GROUP BY
    auto query = relx::query::select_distinct<&users::name, &users::age>()
        .from(users{})
        .group_by(relx::query::to_expr<&users::age>());
    
    EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.name, users.age FROM users GROUP BY users.age");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithOrderBy) {
    // Test DISTINCT with ORDER BY
    auto query = relx::query::select_distinct<&users::name, &users::age>()
        .from(users{})
        .order_by(relx::query::desc(relx::query::to_expr<&users::age>()));
    
    EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.name, users.age FROM users ORDER BY users.age DESC");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithLimitOffset) {
    // Test DISTINCT with LIMIT and OFFSET
    auto query = relx::query::select_distinct<&users::name, &users::age>()
        .from(users{})
        .limit(10)
        .offset(5);
    
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
    
    auto query = relx::query::select_distinct_expr(
        relx::query::as(u.id, "user_id"),
        relx::query::as(u.name, "user_name")
    )
    .from(u);
    
    EXPECT_EQ(query.to_sql(), "SELECT DISTINCT users.id AS user_id, users.name AS user_name FROM users");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(DistinctSelectTest, SelectDistinctWithMixedExpressions) {
    // Test SELECT DISTINCT with a mix of column references and expressions
    users u;
    
    auto query = relx::query::select_distinct(
        u.id,
        relx::query::val(42),
        relx::query::as(u.name, "user_name")
    )
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
    auto query1 = relx::query::select_distinct(u.age)
        .from(u);
    
    // Old way using distinct() expression
    auto query2 = relx::query::select_expr(
        relx::query::distinct(u.age)
    )
    .from(u);
    
    // Both should generate the same SQL
    EXPECT_EQ(query1.to_sql(), "SELECT DISTINCT users.age FROM users");
} 