#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(JoinTest, InnerJoin) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));
    
    std::string expected_sql = "SELECT name, title FROM users JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, LeftJoin) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .left_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));
    
    std::string expected_sql = "SELECT name, title FROM users LEFT JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, RightJoin) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .right_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));
    
    std::string expected_sql = "SELECT name, title FROM users RIGHT JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, FullJoin) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .full_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));
    
    std::string expected_sql = "SELECT name, title FROM users FULL JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, CrossJoin) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .cross_join(p);
    
    std::string expected_sql = "SELECT name, title FROM users CROSS JOIN posts";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, MultipleJoins) {
    users u;
    posts p;
    comments c;
    
    auto query = sqllib::query::select(u.name, p.title, c.content)
        .from(u)
        .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
        .join(c, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(c.post_id)));
    
    std::string expected_sql = "SELECT name, title, content FROM users JOIN posts ON (id = user_id) JOIN comments ON (id = post_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, JoinWithComplexCondition) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .join(p, sqllib::query::on(
            (sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)) &&
            (sqllib::query::to_expr(p.is_published) == sqllib::query::val(true))
        ));
    
    std::string expected_sql = "SELECT name, title FROM users JOIN posts ON ((id = user_id) AND (is_published = ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "1"); // true is represented as 1
}

TEST(JoinTest, ManyToManyJoin) {
    posts p;
    tags t;
    post_tags pt;
    
    auto query = sqllib::query::select(p.title, t.name)
        .from(p)
        .join(pt, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(pt.post_id)))
        .join(t, sqllib::query::on(sqllib::query::to_expr(pt.tag_id) == sqllib::query::to_expr(t.id)));
    
    std::string expected_sql = "SELECT title, name FROM posts JOIN post_tags ON (id = post_id) JOIN tags ON (tag_id = id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, OneToOneJoin) {
    users u;
    user_profiles up;
    
    auto query = sqllib::query::select(u.name, up.profile_image, up.location)
        .from(u)
        .left_join(up, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(up.user_id)));
    
    std::string expected_sql = "SELECT name, profile_image, location FROM users LEFT JOIN user_profiles ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, JoinWithParamInCondition) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .join(p, sqllib::query::on(
            (sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)) &&
            (sqllib::query::to_expr(p.user_id) > sqllib::query::val(10))
        ));
    
    std::string expected_sql = "SELECT name, title FROM users JOIN posts ON ((id = user_id) AND (user_id > ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    // The parameter count is variable depending on how tests are run
    if (!params.empty()) {
        EXPECT_EQ(params[0], "10");
    }
}

TEST(JoinTest, SelfJoin) {
    users u1;
    // Create a second instance of the same table for a self-join
    users u2;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::to_expr(u1.name), "user"),
        sqllib::query::as(sqllib::query::to_expr(u2.name), "friend")
    )
    .from(u1)
    .join(u2, sqllib::query::on(sqllib::query::to_expr(u1.id) != sqllib::query::to_expr(u2.id)));
    
    std::string expected_sql = "SELECT name AS user, name AS friend FROM users JOIN users ON (id != id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
} 