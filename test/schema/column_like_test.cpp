#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>

struct users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<users, "id", int> id;
    relx::schema::column<users, "name", std::string> name;
    relx::schema::column<users, "email", std::string> email;
    relx::schema::column<users, "age", int> age;
    relx::schema::column<users, "is_active", bool> is_active;
};

TEST(ColumnLikeTest, LikeMethod) {
    users u;
    
    // Test the .like() method
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.email.like("%admin%"));
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.email LIKE ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "%admin%");
}

TEST(ColumnLikeTest, IsNullMethod) {
    users u;
    
    // Test the .is_null() method
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.email.is_null());
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.email IS NULL";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 0);
}

TEST(ColumnLikeTest, IsNotNullMethod) {
    users u;
    
    // Test the .is_not_null() method
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.email.is_not_null());
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE users.email IS NOT NULL";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 0);
}

TEST(ColumnLikeTest, CombinedConditions) {
    users u;
    
    // Test combining the new methods with existing operators
    auto query = relx::query::select(u.id, u.name)
        .from(u)
        .where(
            (u.age >= 18 && u.is_active == true) ||
            u.email.like("%admin%")
        );
    
    std::string expected_sql = "SELECT users.id, users.name FROM users WHERE (((users.age >= ?) AND (users.is_active = ?)) OR users.email LIKE ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "1"); // true is represented as 1
    EXPECT_EQ(params[2], "%admin%");
} 