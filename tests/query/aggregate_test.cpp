#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(AggregateTest, CountAll) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::count_all(), "user_count")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(*) AS user_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, CountColumn) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::count(u.id), "user_count")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(users.id) AS user_count FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, CountDistinct) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::count_distinct(u.age), "unique_ages")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(DISTINCT users.age) AS unique_ages FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, Sum) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::sum(u.login_count), "total_logins")
    )
    .from(u);
    
    std::string expected_sql = "SELECT SUM(users.login_count) AS total_logins FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, Average) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::avg(u.age), "average_age")
    )
    .from(u);
    
    std::string expected_sql = "SELECT AVG(users.age) AS average_age FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, MinMax) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::min(u.age), "youngest"),
        relx::query::as(relx::query::max(u.age), "oldest")
    )
    .from(u);
    
    std::string expected_sql = "SELECT MIN(users.age) AS youngest, MAX(users.age) AS oldest FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, MultipleAggregates) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::count_all(), "total_users"),
        relx::query::as(relx::query::avg(u.age), "average_age"),
        relx::query::as(relx::query::sum(u.login_count), "total_logins")
    )
    .from(u);
    
    std::string expected_sql = "SELECT COUNT(*) AS total_users, AVG(users.age) AS average_age, SUM(users.login_count) AS total_logins FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, AggregatesWithWhere) {
    users u;
    
    auto query = relx::query::select_expr(
        relx::query::as(relx::query::count_all(), "active_users"),
        relx::query::as(relx::query::avg(u.age), "average_age")
    )
    .from(u)
    .where(u.is_active == true);
    
    std::string expected_sql = "SELECT COUNT(*) AS active_users, AVG(users.age) AS average_age FROM users WHERE (users.is_active = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "1");
}

TEST(AggregateTest, SimpleGroupBy) {
    users u;
    
    auto query = relx::query::select_expr(
        u.age,
        relx::query::as(relx::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age);
    
    std::string expected_sql = "SELECT users.age, COUNT(*) AS user_count FROM users GROUP BY users.age";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, GroupByMultipleColumns) {
    users u;
    
    auto query = relx::query::select_expr(
        u.age,
        u.is_active,
        relx::query::as(relx::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age, u.is_active);
    
    std::string expected_sql = "SELECT users.age, users.is_active, COUNT(*) AS user_count FROM users GROUP BY users.age, users.is_active";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, GroupByWithHaving) {
    users u;
    
    auto query = relx::query::select_expr(
        u.age,
        relx::query::as(relx::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age)
    .having(relx::query::count_all() > 5);
    
    std::string expected_sql = "SELECT users.age, COUNT(*) AS user_count FROM users GROUP BY users.age HAVING (COUNT(*) > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "5");
}

TEST(AggregateTest, GroupByWithHavingAndWhere) {
    posts p;
    
    auto query = relx::query::select_expr(
        p.user_id,
        relx::query::as(relx::query::count_all(), "post_count"),
        relx::query::as(relx::query::sum(p.views), "total_views")
    )
    .from(p)
    .where(p.is_published == true)
    .group_by(p.user_id)
    .having(relx::query::sum(p.views) > 1000);
    
    std::string expected_sql = "SELECT posts.user_id, COUNT(*) AS post_count, SUM(posts.views) AS total_views FROM posts WHERE (posts.is_published = ?) GROUP BY posts.user_id HAVING (SUM(posts.views) > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "1");
    EXPECT_EQ(params[1], "1000");
}

TEST(AggregateTest, GroupByWithOrderBy) {
    users u;
    
    auto query = relx::query::select_expr(
        u.age,
        relx::query::as(relx::query::count_all(), "user_count")
    )
    .from(u)
    .group_by(u.age)
    .order_by(relx::query::desc(relx::query::count_all()));
    
    std::string expected_sql = "SELECT users.age, COUNT(*) AS user_count FROM users GROUP BY users.age ORDER BY COUNT(*) DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(AggregateTest, JoinWithGroupBy) {
    users u;
    posts p;
    
    auto query = relx::query::select_expr(
        u.id,
        u.name,
        relx::query::as(relx::query::count(p.id), "post_count")
    )
    .from(u)
    .left_join(p, relx::query::on(u.id == p.user_id))
    .group_by(u.id, u.name)
    .order_by(relx::query::desc(relx::query::count(p.id)));
    
    std::string expected_sql = "SELECT users.id, users.name, COUNT(posts.id) AS post_count FROM users LEFT JOIN posts ON (users.id = posts.user_id) GROUP BY users.id, users.name ORDER BY COUNT(posts.id) DESC";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
} 