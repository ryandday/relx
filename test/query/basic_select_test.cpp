#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(BasicSelectTest, SimpleSelect) {
    users u;
    
    auto query = relx::query::select(u.id, u.name, u.email)
        .from(u);
    
    std::string expected_sql = "SELECT users.id, users.name, users.email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectAllColumns) {
    // Using the new API with member pointers
    auto query = relx::query::select<
                    &users::id, &users::name, &users::email, &users::age,
                    &users::created_at, &users::is_active, &users::bio, &users::login_count
                >()
                .from(users{});
    
    std::string expected_sql = "SELECT users.id, users.name, users.email, users.age, users.created_at, users.is_active, users.bio, users.login_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithExplicitTableName) {
    // Using the new API with member pointers from multiple tables
    auto query = relx::query::select<&users::id, &posts::id>()
        .from(users{})
        .from(posts{});
    
    std::string expected_sql = "SELECT users.id, posts.id FROM users, posts";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithColumnAliases) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(u.id, "user_id"),
        relx::query::as(u.name, "user_name"),
        relx::query::as(u.email, "user_email")
    )
    .from(u);
    
    std::string expected_sql = "SELECT users.id AS user_id, users.name AS user_name, users.email AS user_email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLiteral) {
    users u;
    
    auto query = relx::query::select_expr(
        u.id,
        relx::query::val(42),
        relx::query::val("constant string")
    )
    .from(u);
    
    std::string expected_sql = "SELECT users.id, ?, ? FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "42");
    EXPECT_EQ(params[1], "constant string");
}

TEST(BasicSelectTest, SelectWithDistinct) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::distinct(u.age)
    )
    .from(u);
    
    std::string expected_sql = "SELECT DISTINCT users.age FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLimit) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .limit(10);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "10");
}

TEST(BasicSelectTest, SelectWithLimitAndOffset) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .limit(10)
        .offset(20);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ? OFFSET ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "10");
    EXPECT_EQ(params[1], "20");
}

TEST(BasicSelectTest, SelectWithOrderByAsc) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .order_by(relx::query::asc(u.name));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithOrderByDesc) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .order_by(relx::query::desc(u.age));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.age DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithMultipleOrderBy) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .order_by(
            relx::query::desc(u.age),
            relx::query::asc(u.name)
        );
    
    std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.age DESC, users.name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithLimitNew) {
    // Using the new API with member pointers and limit
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .limit(10);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "10");
}

TEST(BasicSelectTest, SelectWithLimitAndOffsetNew) {
    // Using the new API with member pointers, limit and offset
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .limit(10)
        .offset(20);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ? OFFSET ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "10");
    EXPECT_EQ(params[1], "20");
}

TEST(BasicSelectTest, SelectWithOrderByAscNew) {
    // Using the new API with member pointers and order by
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .order_by(relx::query::asc(relx::query::to_expr<&users::name>()));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithOrderByDescNew) {
    // Using the new API with member pointers and order by desc
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .order_by(relx::query::desc(relx::query::to_expr<&users::age>()));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.age DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithMultipleOrderByNew) {
    // Using the new API with member pointers and multiple order by clauses
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .order_by(
            relx::query::desc(relx::query::to_expr<&users::age>()),
            relx::query::asc(relx::query::to_expr<&users::name>())
        );
    
    std::string expected_sql = "SELECT users.id, users.name FROM users ORDER BY users.age DESC, users.name ASC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectWithConditionNew) {
    // Using the new API with member pointers and WHERE conditions
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .where(relx::query::to_expr<&users::age>() > 18);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.age > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(BasicSelectTest, SelectWithMultipleConditionsNew) {
    // Using the new API with member pointers and multiple WHERE conditions
    auto query = relx::query::select<&users::id, &users::name>()
        .from(users{})
        .where(relx::query::to_expr<&users::age>() >= 18 && 
               relx::query::to_expr<&users::name>() != "");
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND (users.name != ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "");
}

TEST(BasicSelectTest, SelectWithJoinNew) {
    // Using the new API with member pointers and join
    users u;
    posts p;
    auto query = relx::query::select<&users::name, &posts::title>()
        .from(u)
        .join(p, relx::query::on(
            u.id == p.user_id
        ));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users JOIN posts ON (users.id = posts.user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(BasicSelectTest, SelectFromHelper) {
    // Using the select_from helper to create a query with FROM clause in one step
    auto query = relx::query::select_from<&users::id, &users::name, &users::email>();
    
    std::string expected_sql = "SELECT users.id, users.name, users.email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
    
    users u;
    // Add a WHERE condition to make sure it works with other methods
    auto query_with_where = query.where(u.age > 18);
    std::string expected_where_sql = "SELECT users.id, users.name, users.email FROM users WHERE (users.age > ?)";
    EXPECT_EQ(query_with_where.to_sql(), expected_where_sql);
    
    auto params = query_with_where.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
} 
