#include <gtest/gtest.h>
#include <relx/schema.hpp>

using namespace relx::schema;

// Test table with standard primary key
struct User {
    static constexpr auto table_name = "users";
    
    column<"id", int> id;
    column<"username", std::string> username;
    
    primary_key<&User::id> pk;
};

// Test table with composite primary key
struct SessionData {
    static constexpr auto table_name = "session_data";
    
    column<"user_id", int> user_id;
    column<"session_id", std::string> session_id;
    column<"key", std::string> key;
    column<"value", std::string> value;
    
    // Note: composite primary key may not be fully implemented in the library yet
    // This is a test for the intended functionality
    composite_primary_key<&SessionData::user_id, &SessionData::session_id, &SessionData::key> pk;
};

TEST(PrimaryKeyTest, BasicPrimaryKey) {
    User user;
    
    // Test the SQL definition of a simple primary key
    EXPECT_EQ(user.pk.sql_definition(), "PRIMARY KEY (id)");
    
    // Test that the primary key appears in the table constraints
    std::string constraints = collect_constraint_definitions(user);
    EXPECT_TRUE(constraints.find("PRIMARY KEY (id)") != std::string::npos);
    
    // Test that the primary key appears in the CREATE TABLE statement
    std::string sql = create_table(user);
    EXPECT_TRUE(sql.find("PRIMARY KEY (id)") != std::string::npos);
}

TEST(PrimaryKeyTest, CompositePrimaryKey) {
    SessionData session;
    
    // Test the SQL definition of a composite primary key
    EXPECT_EQ(session.pk.sql_definition(), "PRIMARY KEY (user_id, session_id, key)");
    
    // Test that the composite primary key appears in the table constraints
    std::string constraints = collect_constraint_definitions(session);
    EXPECT_TRUE(constraints.find("PRIMARY KEY (user_id, session_id, key)") != std::string::npos);
    
    // Test that the composite primary key appears in the CREATE TABLE statement
    std::string sql = create_table(session);
    EXPECT_TRUE(sql.find("PRIMARY KEY (user_id, session_id, key)") != std::string::npos);
} 
