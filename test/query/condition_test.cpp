#include <gtest/gtest.h>
#include "test_common.hpp"

using namespace test_tables;
using namespace test_utils;

TEST(ConditionTest, SimpleEquality) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.id == 1);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.id = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "1");
}

TEST(ConditionTest, SimpleInequality) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.age > 21);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.age > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "21");
}

TEST(ConditionTest, LogicalAnd) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(
            (u.age >= 18) && 
            (u.is_active == true)
        );
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND (users.is_active = ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "1"); // true is represented as 1
}

TEST(ConditionTest, LogicalOr) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(
            (u.age < 18) || 
            (u.age >= 65)
        );
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE ((users.age < ?) OR (users.age >= ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "65");
}

TEST(ConditionTest, LogicalNotValue) {
    users u;
    
    // Instead of negating the column directly, compare with false
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.is_active == false);
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (users.is_active = ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "0"); // false is represented as 0
}

TEST(ConditionTest, ComplexLogicalExpression) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(
            (u.age >= 18) && 
            (u.is_active == true || 
             u.login_count > 10)
        );
    
    // Note: testing exact parentheses nesting is implementation-dependent
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND ((users.is_active = ?) OR (users.login_count > ?)))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "1"); // true is represented as 1
    EXPECT_EQ(params[2], "10");
}

TEST(ConditionTest, StringLike) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(relx::query::like(u.email, "%@example.com"));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.email LIKE ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "%@example.com");
}

TEST(ConditionTest, StringNotLike) {
    users u;
    
    // Use operator! directly to negate the condition
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(!(relx::query::like(u.email, "%@example.com")));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (NOT users.email LIKE ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "%@example.com");
}

TEST(ConditionTest, InList) {
    users u;
    
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    auto query = relx::query::select(u.id, u.email)
        .from(u)
        .where(relx::query::in(u.name, names));
    
    std::string expected_sql = "SELECT users.id, users.email FROM users WHERE users.name IN (?, ?, ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "Alice");
    EXPECT_EQ(params[1], "Bob");
    EXPECT_EQ(params[2], "Charlie");
}

TEST(ConditionTest, NotInList) {
    users u;
    
    // Convert integer values to strings for the 'in' operator
    std::vector<std::string> age_strings = {"18", "21", "25"};
    
    // Use operator! directly to negate the condition
    auto query = relx::query::select(u.id, u.email)
        .from(u)
        .where(!(relx::query::in(u.age, age_strings)));
    
    std::string expected_sql = "SELECT users.id, users.email FROM users WHERE (NOT users.age IN (?, ?, ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "21");
    EXPECT_EQ(params[2], "25");
}

TEST(ConditionTest, IsNull) {
    users u;
    
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(relx::query::is_null(u.bio));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.bio IS NULL";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(ConditionTest, IsNotNull) {
    users u;
    
    // Use is_not_null directly
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(relx::query::is_not_null(u.bio));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.bio IS NOT NULL";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(ConditionTest, Between) {
    users u;
    
    // Use string values for between
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(relx::query::between(u.age, "18", "65"));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.age BETWEEN ? AND ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "65");
}

TEST(ConditionTest, NotBetween) {
    users u;
    
    // Use operator! directly to negate the condition
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(!(relx::query::between(u.age, "18", "65")));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (NOT users.age BETWEEN ? AND ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "65");
} 