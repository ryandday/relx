#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(BasicSelectTest, SimpleSelect) {
    // Using the new API with member pointers
    auto query = sqllib::query::select<&users::id, &users::name, &users::email>()
        .from(users{});
    
    std::string expected_sql = "SELECT id, name, email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SimpleSelectLegacy) {
    // Using the legacy API for comparison
    users u;
    
    auto query = sqllib::query::select(u.id, u.name, u.email)
        .from(u);
    
    std::string expected_sql = "SELECT id, name, email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectAllColumns) {
    // Using the new API with member pointers
    auto query = sqllib::query::select<
                    &users::id, &users::name, &users::email, &users::age,
                    &users::created_at, &users::is_active, &users::bio, &users::login_count
                >()
                .from(users{});
    
    std::string expected_sql = "SELECT id, name, email, age, created_at, is_active, bio, login_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithExplicitTableName) {
    // Using the new API with member pointers from multiple tables
    auto query = sqllib::query::select<&users::id, &posts::id>()
        .from(users{})
        .from(posts{});
    
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

TEST(BasicSelectTest, SelectWithLimitNew) {
    // Using the new API with member pointers and limit
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .limit(10);
    
    std::string expected_sql = "SELECT id, name FROM users LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "10");
}

TEST(BasicSelectTest, SelectWithLimitAndOffsetNew) {
    // Using the new API with member pointers, limit and offset
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .limit(10)
        .offset(20);
    
    std::string expected_sql = "SELECT id, name FROM users LIMIT ? OFFSET ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "10");
    EXPECT_EQ(params[1], "20");
}

TEST(BasicSelectTest, SelectWithOrderByAscNew) {
    // Using the new API with member pointers and order by
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .order_by(sqllib::query::asc(sqllib::query::to_expr<&users::name>()));
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithOrderByDescNew) {
    // Using the new API with member pointers and order by desc
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .order_by(sqllib::query::desc(sqllib::query::to_expr<&users::age>()));
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY age DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithMultipleOrderByNew) {
    // Using the new API with member pointers and multiple order by clauses
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .order_by(
            sqllib::query::desc(sqllib::query::to_expr<&users::age>()),
            sqllib::query::asc(sqllib::query::to_expr<&users::name>())
        );
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY age DESC, name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithConditionNew) {
    // Using the new API with member pointers and WHERE conditions
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .where(sqllib::query::to_expr<&users::age>() > sqllib::query::val(18));
    
    std::string expected_sql = "SELECT id, name FROM users WHERE (age > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(BasicSelectTest, SelectWithMultipleConditionsNew) {
    // Using the new API with member pointers and multiple WHERE conditions
    auto query = sqllib::query::select<&users::id, &users::name>()
        .from(users{})
        .where(sqllib::query::to_expr<&users::age>() >= sqllib::query::val(18) && 
               sqllib::query::to_expr<&users::name>() != sqllib::query::val(""));
    
    std::string expected_sql = "SELECT id, name FROM users WHERE ((age >= ?) AND (name != ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "");
}

TEST(BasicSelectTest, SelectWithJoinNew) {
    // Using the new API with member pointers and join
    auto query = sqllib::query::select<&users::name, &posts::title>()
        .from(users{})
        .join(posts{}, sqllib::query::on(
            sqllib::query::to_expr<&users::id>() == sqllib::query::to_expr<&posts::user_id>()
        ));
    
    std::string expected_sql = "SELECT name, title FROM users JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectFromHelper) {
    // Using the select_from helper to create a query with FROM clause in one step
    auto query = sqllib::query::select_from<&users::id, &users::name, &users::email>();
    
    std::string expected_sql = "SELECT id, name, email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
    
    // Add a WHERE condition to make sure it works with other methods
    auto query_with_where = query.where(sqllib::query::to_expr<&users::age>() > sqllib::query::val(18));
    std::string expected_where_sql = "SELECT id, name, email FROM users WHERE (age > ?)";
    EXPECT_EQ(query_with_where.to_sql(), expected_where_sql);
    
    auto params = query_with_where.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
} 