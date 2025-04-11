#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(AggregateTest, CountAll) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count_all(), "user_count")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(*) AS user_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, CountColumn) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count(u.id), "user_count")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(id) AS user_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, CountDistinct) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count_distinct(u.age), "unique_ages")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(DISTINCT age) AS unique_ages FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, Sum) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::sum(u.login_count), "total_logins")
    )
    .from(u);
    
    std::string expected_sql = "SELECT SUM(login_count) AS total_logins FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, Average) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::avg(u.age), "average_age")
    )
    .from(u);
    
    std::string expected_sql = "SELECT AVG(age) AS average_age FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, MinMax) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::min(u.age), "youngest"),
        sqllib::query::as(sqllib::query::max(u.age), "oldest")
    )
    .from(u);
    
    std::string expected_sql = "SELECT MIN(age) AS youngest, MAX(age) AS oldest FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, MultipleAggregates) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count_all(), "total_users"),
        sqllib::query::as(sqllib::query::avg(u.age), "average_age"),
        sqllib::query::as(sqllib::query::sum(u.login_count), "total_logins")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(*) AS total_users, AVG(age) AS average_age, SUM(login_count) AS total_logins FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, AggregatesWithWhere) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count_all(), "active_users"),
        sqllib::query::as(sqllib::query::avg(u.age), "average_age")
    )
    .from(u)
    .where(u.is_active == true);
    
    std::string expected_sql = "SELECT COUNT(*) AS active_users, AVG(age) AS average_age FROM users WHERE (is_active = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "1");
}

TEST(AggregateTest, SimpleGroupBy) {
    users u;
    
    auto query = sqllib::query::select_expr(
        u.age,
        sqllib::query::as(sqllib::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age);
    
    std::string expected_sql = "SELECT age, COUNT(*) AS user_count FROM users GROUP BY age";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, GroupByMultipleColumns) {
    users u;
    
    auto query = sqllib::query::select_expr(
        u.age,
        u.is_active,
        sqllib::query::as(sqllib::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age, u.is_active);
    
    std::string expected_sql = "SELECT age, is_active, COUNT(*) AS user_count FROM users GROUP BY age, is_active";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, GroupByWithHaving) {
    users u;
    
    auto query = sqllib::query::select_expr(
        u.age,
        sqllib::query::as(sqllib::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age)
    .having(sqllib::query::count_all() > 5);
    
    std::string expected_sql = "SELECT age, COUNT(*) AS user_count FROM users GROUP BY age HAVING (COUNT(*) > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "5");
}

TEST(AggregateTest, GroupByWithHavingAndWhere) {
    posts p;
    
    auto query = sqllib::query::select_expr(
        p.user_id,
        sqllib::query::as(sqllib::query::count_all(), "post_count"),
        sqllib::query::as(sqllib::query::sum(p.views), "total_views")
    )
    .from(p)
    .where(p.is_published == true)
    .group_by(p.user_id)
    .having(sqllib::query::sum(p.views) > 1000);
    
    std::string expected_sql = "SELECT user_id, COUNT(*) AS post_count, SUM(views) AS total_views FROM posts WHERE (is_published = ?) GROUP BY user_id HAVING (SUM(views) > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "1");
    EXPECT_EQ(params[1], "1000");
}

TEST(AggregateTest, GroupByWithOrderBy) {
    users u;
    
    auto query = sqllib::query::select_expr(
        u.age,
        sqllib::query::as(sqllib::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age)
    .order_by(sqllib::query::desc(sqllib::query::count_all()));
    
    std::string expected_sql = "SELECT age, COUNT(*) AS user_count FROM users GROUP BY age ORDER BY COUNT(*) DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, JoinWithGroupBy) {
    users u;
    posts p;
    
    auto query = sqllib::query::select_expr(
        u.id,
        u.name,
        sqllib::query::as(sqllib::query::count(p.id), "post_count")
    )
    .from(u)
    .left_join(p, sqllib::query::on(u.id == p.user_id))
    .group_by(u.id, u.name)
    .order_by(sqllib::query::desc(sqllib::query::count(p.id)));
    
    std::string expected_sql = "SELECT id, name, COUNT(id) AS post_count FROM users LEFT JOIN posts ON (id = user_id) GROUP BY id, name ORDER BY COUNT(id) DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
} 