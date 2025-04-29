#include <gtest/gtest.h>
#include "test_common.hpp"
#include <string>
#include <limits>

using namespace test_tables;
using namespace test_utils;

TEST(EdgeCaseTest, ExtremeLimits) {
    users u;
    
    // Test with very large LIMIT value
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .limit(std::numeric_limits<int>::max());
    
    std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], std::to_string(std::numeric_limits<int>::max()));
}

TEST(EdgeCaseTest, ZeroValues) {
    users u;
    
    // Test with zero LIMIT (should be handled gracefully)
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .limit(0);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "0");
}

TEST(EdgeCaseTest, EmptyStrings) {
    users u;
    
    // Test with empty string parameters
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.name == "");
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.name = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "");
}

TEST(EdgeCaseTest, SpecialCharactersInStrings) {
    users u;
    
    // Test with strings containing SQL special characters
    std::string special_chars = "Test'\"\\%;_$#@!";
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.name == special_chars);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.name = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], special_chars); // The string should be passed as-is to parameters
}

TEST(EdgeCaseTest, UnicodeStrings) {
    users u;
    
    // Test with Unicode strings
    std::string unicode_string = "测试Unicode字符串😀🔥";
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.name == unicode_string);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.name = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], unicode_string);
}

TEST(EdgeCaseTest, VeryLongStrings) {
    users u;
    
    // Create a very long string value
    std::string long_string(10000, 'a');
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.bio == long_string);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.bio = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], long_string);
}

// Let's create a class for column to test
class TestFloatColumn {
public:
    static constexpr auto column_name = "float_column";
    using value_type = float;
};

TEST(EdgeCaseTest, BooleanValues) {
    users u;
    
    // Test with boolean values
    auto query_true = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.is_active == true);
    
    auto query_false = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.is_active == false);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.is_active = ?)";
    EXPECT_EQ(query_true.to_sql(), expected_sql);
    EXPECT_EQ(query_false.to_sql(), expected_sql);
    
    auto params_true = query_true.bind_params();
    auto params_false = query_false.bind_params();
    
    EXPECT_EQ(params_true.size(), 1);
    EXPECT_EQ(params_false.size(), 1);
    // The exact string representation of true/false may vary by implementation
    // Common values are "1"/"0" or "true"/"false"
    EXPECT_TRUE(params_true[0] == "1" || params_true[0] == "true" || params_true[0] == "TRUE");
    EXPECT_TRUE(params_false[0] == "0" || params_false[0] == "false" || params_false[0] == "FALSE");
}

TEST(EdgeCaseTest, ExtremeDateValues) {
    posts p;
    
    // Test with extreme date strings
    std::string min_date = "0001-01-01 00:00:00";
    std::string max_date = "9999-12-31 23:59:59";
    
    auto query_min = relx::query::select(p.id, p.title)
        .from(p)
        .where(p.created_at == min_date);
    
    auto query_max = relx::query::select(p.id, p.title)
        .from(p)
        .where(p.created_at == max_date);
    
    std::string expected_sql = "SELECT posts.id, posts.title FROM posts WHERE (posts.created_at = ?)";
    EXPECT_EQ(query_min.to_sql(), expected_sql);
    EXPECT_EQ(query_max.to_sql(), expected_sql);
    
    auto params_min = query_min.bind_params();
    auto params_max = query_max.bind_params();
    
    EXPECT_EQ(params_min.size(), 1);
    EXPECT_EQ(params_max.size(), 1);
    EXPECT_EQ(params_min[0], min_date);
    EXPECT_EQ(params_max[0], max_date);
}

TEST(EdgeCaseTest, ComplexExpressionsWithManyOperators) {
    users u;
    posts p;
    
    // Create a complex WHERE condition with many operators
    auto query = relx::query::select(u.id, u.name, p.title)
        .from(u)
        .join(p, relx::query::on(u.id == p.user_id))
        .where(
            (u.age > 18) &&
            (u.age <= 65) &&
            (u.is_active == true) &&
            ((u.name != "") || 
             (p.views > 1000))
        );
    
    // Just test that the query produces a valid SQL string without error
    std::string sql = query.to_sql();
    EXPECT_FALSE(sql.empty());
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 5);
    
    // The generated SQL should include all operators
    EXPECT_TRUE(sql.find(">") != std::string::npos);
    EXPECT_TRUE(sql.find("<=") != std::string::npos);
    EXPECT_TRUE(sql.find("=") != std::string::npos);
    EXPECT_TRUE(sql.find("!=") != std::string::npos);
    EXPECT_TRUE(sql.find("AND") != std::string::npos);
    EXPECT_TRUE(sql.find("OR") != std::string::npos);
}

TEST(EdgeCaseTest, NestedLogicalOperators) {
    users u;
    
    // Create deeply nested logical operators
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(
            u.is_active == true &&
            (
                (u.age < 30 || 
                 u.age > 60) &&
                (u.login_count > 5 || 
                 relx::query::is_not_null(u.bio))
            )
        );
    
    // The SQL should have proper nesting of conditions with parentheses
    std::string sql = query.to_sql();
    EXPECT_FALSE(sql.empty());
    
    // Count opening and closing parentheses - they should match
    size_t open_count = 0;
    size_t close_count = 0;
    
    for (char c : sql) {
        if (c == '(') open_count++;
        if (c == ')') close_count++;
    }
    
    EXPECT_EQ(open_count, close_count);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 4);
}

// TEST(EdgeCaseTest, ManyJoinsAndConditions) {
//     users u;
//     posts p;
//     comments c;
//     tags t;
//     post_tags pt;
    
//     // Create a query with many joins and conditions
//     auto query = relx::query::select(u.name, p.title, c.content, t.name)
//         .from(u)
//         .join(p, relx::query::on(relx::query::to_expr(u.id) == relx::query::to_expr(p.user_id)))
//         .join(c, relx::query::on(relx::query::to_expr(p.id) == relx::query::to_expr(c.post_id)))
//         .join(pt, relx::query::on(relx::query::to_expr(p.id) == relx::query::to_expr(pt.post_id)))
//         .join(t, relx::query::on(relx::query::to_expr(pt.tag_id) == relx::query::to_expr(t.id)))
//         .where(relx::query::to_expr(u.is_active) == relx::query::val(true))
//         .where(relx::query::to_expr(p.is_published) == relx::query::val(true))
//         .where(relx::query::to_expr(c.is_approved) == relx::query::val(true));
    
//     // Just verify we get a non-empty string with all the expected JOIN keywords
//     std::string sql = query.to_sql();
//     EXPECT_FALSE(sql.empty());
    
//     // Count the number of JOIN statements
//     size_t join_count = 0;
//     size_t pos = 0;
//     while ((pos = sql.find("JOIN", pos)) != std::string::npos) {
//         join_count++;
//         pos += 4; // Move past "JOIN"
//     }
    
//     EXPECT_EQ(join_count, 4);
    
//     auto params = query.bind_params();
//     EXPECT_EQ(params.size(), 3);
// } 
