#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(JoinTest, InnerJoin) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .join(p, relx::query::on(u.id == p.user_id));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users JOIN posts ON (users.id = posts.user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, LeftJoin) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .left_join(p, relx::query::on(u.id == p.user_id));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users LEFT JOIN posts ON (users.id = posts.user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, RightJoin) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .right_join(p, relx::query::on(u.id == p.user_id));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users RIGHT JOIN posts ON (users.id = posts.user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, FullJoin) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .full_join(p, relx::query::on(u.id == p.user_id));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users FULL JOIN posts ON (users.id = posts.user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, CrossJoin) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .cross_join(p);
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users CROSS JOIN posts";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, MultipleJoins) {
    users u;
    posts p;
    comments c;
    
    auto query = relx::query::select(u.name, p.title, c.content)
        .from(u)
        .join(p, relx::query::on(u.id == p.user_id))
        .join(c, relx::query::on(p.id == c.post_id));
    
    std::string expected_sql = "SELECT users.name, posts.title, comments.content FROM users JOIN posts ON (users.id = posts.user_id) JOIN comments ON (posts.id = comments.post_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, JoinWithComplexCondition) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .join(p, relx::query::on(
            (u.id == p.user_id) &&
            (p.is_published == true)
        ));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users JOIN posts ON ((users.id = posts.user_id) AND (posts.is_published = ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    // The parameter count is variable depending on how tests are run
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "1"); // true is represented as 1
}

TEST(JoinTest, ManyToManyJoin) {
    posts p;
    tags t;
    post_tags pt;
    
    auto query = relx::query::select(p.title, t.name)
        .from(p)
        .join(pt, relx::query::on(p.id == pt.post_id))
        .join(t, relx::query::on(pt.tag_id == t.id));
    
    std::string expected_sql = "SELECT posts.title, tags.name FROM posts JOIN post_tags ON (posts.id = post_tags.post_id) JOIN tags ON (post_tags.tag_id = tags.id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, OneToOneJoin) {
    users u;
    user_profiles up;
    
    auto query = relx::query::select(u.name, up.profile_image, up.location)
        .from(u)
        .left_join(up, relx::query::on(u.id == up.user_id));
    
    std::string expected_sql = "SELECT users.name, user_profiles.profile_image, user_profiles.location FROM users LEFT JOIN user_profiles ON (users.id = user_profiles.user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(JoinTest, JoinWithParamInCondition) {
    users u;
    posts p;
    
    auto query = relx::query::select(u.name, p.title)
        .from(u)
        .join(p, relx::query::on(
            (u.id == p.user_id) &&
            (p.user_id > 10)
        ));
    
    std::string expected_sql = "SELECT users.name, posts.title FROM users JOIN posts ON ((users.id = posts.user_id) AND (posts.user_id > ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    // The parameter count is variable depending on how tests are run
    EXPECT_EQ(params[0], "10");
    EXPECT_EQ(params.size(), 1);
}

TEST(JoinTest, SelfJoin) {
    users u1;
    // Create a second instance of the same table for a self-join
    users u2;
    
    auto query = relx::query::select_expr(
        relx::query::as(u1.name, "user"),
        relx::query::as(u2.name, "friend")
    )
    .from(u1)
    .join(u2, relx::query::on(u1.id != u2.id));
    
    std::string expected_sql = "SELECT users.name AS user, users.name AS friend FROM users JOIN users ON (users.id != users.id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
} 