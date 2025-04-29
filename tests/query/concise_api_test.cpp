#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;
using namespace relx::query::literals;

TEST(ConciseApiTest, DirectComparisonOperators) {
    users u;
    
    // Test direct comparison without to_expr and val
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.age > 18);
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users WHERE (users.age > ?)");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(ConciseApiTest, MultipleConditionsWithDirectComparison) {
    users u;
    
    // Test multiple conditions with direct comparisons
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where((u.age >= 18) && (u.is_active == true));
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND (users.is_active = ?))");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    // The exact representation of bool may vary
    EXPECT_TRUE(params[1] == "1" || params[1] == "true" || params[1] == "TRUE");
}

TEST(ConciseApiTest, DirectColumnComparison) {
    users u;
    posts p;
    
    // Test direct comparison between columns
    auto query = relx::query::select(u.id, u.name, p.title)
        .from(u)
        .join(p, relx::query::on(u.id == p.user_id));
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name, posts.title FROM users JOIN posts ON (users.id = posts.user_id)");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(ConciseApiTest, SQLLiterals) {
    users u;
    
    // Test SQL literals with _sql suffix
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where((u.age > 18_sql) && (u.name != "John"_sql));
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users WHERE ((users.age > ?) AND (users.name != ?))");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "John");
}

TEST(ConciseApiTest, ShorthandHelpers) {
    using namespace relx::query;
    users u;
    
    // Test shorthand helpers
    auto query = select(u.id, u.name)
        .from(u)
        .where(e(u.age) > v(18));
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users WHERE (users.age > ?)");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(ConciseApiTest, ShorthandAggregates) {
    using namespace relx::query;
    users u;
    
    // Test shorthand aggregate functions
    auto query = select_expr(
        a(c_all(), "user_count"),
        a(a_avg(e(u.age)), "average_age"),
        a(s(e(u.login_count)), "total_logins")
    )
    .from(u)
    .where(u.is_active == true);
    
    EXPECT_EQ(query.to_sql(), "SELECT COUNT(*) AS user_count, AVG(users.age) AS average_age, SUM(users.login_count) AS total_logins FROM users WHERE (users.is_active = ?)");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    // The exact representation of bool may vary
    EXPECT_TRUE(params[0] == "1" || params[0] == "true" || params[0] == "TRUE");
}

TEST(ConciseApiTest, ShorthandOrderBy) {
    using namespace relx::query;
    users u;
    
    // Test shorthand order by functions
    auto query = select(u.id, u.name)
        .from(u)
        .order_by(a_by(e(u.name)), d_by(e(u.age)));
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users ORDER BY users.name ASC, users.age DESC");
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(ConciseApiTest, MixConciseAndFullApi) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where((u.age > 18) && 
               (relx::query::to_expr(u.email) != relx::query::val("")));
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users WHERE ((users.age > ?) AND (users.email != ?))");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "");
}

TEST(ConciseApiTest, ComplexQuery) {
    using namespace relx::query;
    
    users u;
    posts p;
    
    // Test a complex query with all concise features
    auto query = select(u.id, u.name, a(c(e(p.id)), "post_count"))
        .from(u)
        .left_join(p, on(u.id == p.user_id))
        .where((u.age >= 18_sql) && (u.is_active == true))
        .group_by(e(u.id), e(u.name))
        .having(c(e(p.id)) > v(0))
        .order_by(d_by(e(u.age)))
        .limit(10)
        .offset(20);
    
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name, COUNT(posts.id) AS post_count FROM users LEFT JOIN posts ON (users.id = posts.user_id) WHERE ((users.age >= ?) AND (users.is_active = ?)) GROUP BY users.id, users.name HAVING (COUNT(posts.id) > ?) ORDER BY users.age DESC LIMIT ? OFFSET ?");
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 5);
    EXPECT_EQ(params[0], "18");
    // The exact representation of bool may vary
    EXPECT_TRUE(params[1] == "1" || params[1] == "true" || params[1] == "TRUE");
    EXPECT_EQ(params[2], "0");
    EXPECT_EQ(params[3], "10");
    EXPECT_EQ(params[4], "20");
} 
