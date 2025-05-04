#include <gtest/gtest.h>
#include <relx/postgresql.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <relx/connection/connection.hpp>
#include <boost/pfr.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <memory>

namespace {

// Define a simple test table
struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<Users, "id", int, relx::identity<>> id;
    relx::schema::column<Users, "name", std::string> name;
    relx::schema::column<Users, "email", std::string> email;
    relx::schema::column<Users, "age", int> age;
    relx::schema::column<Users, "active", bool> active;
    relx::schema::column<Users, "score", double> score;
    
    relx::schema::table_primary_key<&Users::id> pk;
};

// Define a DTO that matches the table columns
struct UserDTO {
    int id;
    std::string name;
    std::string email;
    int age;
    bool active;
    double score;
};

// Define a partial DTO with fewer fields
struct PartialUserDTO {
    int id;
    std::string name;
    int age;
};

// // Define a DTO with fields in different order
// struct UserDTODifferentOrder {
//     std::string name;
//     int id;
//     std::string email;
//     bool active;
//     int age;
//     double score;
// };

class DtoMappingIntegrationTest : public ::testing::Test {
protected:
    // Connection string for the PostgreSQL test database
    std::string conn_string = "host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres";
    std::unique_ptr<relx::PostgreSQLConnection> conn;
    Users users;
    
    void SetUp() override {
        // Connect to the database
        conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
        auto connect_result = conn->connect();
        ASSERT_TRUE(connect_result) << "Failed to connect to database: " << connect_result.error().message;
        
        // Clean up any existing test tables
        clean_test_table();
        
        // Create the test table
        create_test_table();
        
        // Insert test data
        insert_test_data();
    }
    
    void TearDown() override {
        if (conn && conn->is_connected()) {
            clean_test_table();
            conn->disconnect();
        }
    }
    
    // Helper to clean up the test table
    void clean_test_table() {
        auto drop_sql = relx::drop_table(users).if_exists().cascade();
        auto result = conn->execute(drop_sql);
        ASSERT_TRUE(result) << "Failed to drop table: " << result.error().message;
    }
    
    // Helper to create the test table
    void create_test_table() {
        auto create_sql = relx::create_table(users);
        auto result = conn->execute(create_sql);
        ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
    }
    
    // Helper to insert test data
    void insert_test_data() {
        // Insert multiple rows
        auto insert_query = relx::query::insert_into(users)
            .columns(users.name, users.email, users.age, users.active, users.score)
            .values("John Doe", "john@example.com", 30, true, 85.5)
            .values("Jane Smith", "jane@example.com", 28, true, 92.3)
            .values("Bob Johnson", "bob@example.com", 35, false, 78.9)
            .values("Alice Brown", "alice@example.com", 42, true, 91.7)
            .values("Charlie Davis", "charlie@example.com", 25, false, 68.2);
            
        auto result = conn->execute(insert_query);
        ASSERT_TRUE(result) << "Failed to insert test data: " << result.error().message;
    }
};

// Test mapping a complete DTO with exact field match
TEST_F(DtoMappingIntegrationTest, CompleteStructMapping) {
    // Create a select query
    auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
        .from(users)
        .where(users.id == 1);
    
    // Execute the query with DTO mapping
    auto result = conn->execute<UserDTO>(query);
    
    // Verify result
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    // Check mapped struct fields
    UserDTO user = *result;
    EXPECT_EQ(1, user.id);
    EXPECT_EQ("John Doe", user.name);
    EXPECT_EQ("john@example.com", user.email);
    EXPECT_EQ(30, user.age);
    EXPECT_TRUE(user.active);
    EXPECT_DOUBLE_EQ(85.5, user.score);
}

// Test mapping to a partial DTO with fewer fields
TEST_F(DtoMappingIntegrationTest, PartialStructMapping) {
    // Create a select query with only the fields needed for the partial DTO
    auto query = relx::query::select(users.id, users.name, users.age)
        .from(users)
        .where(users.id == 2);
    
    // Execute the query with partial DTO mapping
    auto result = conn->execute<PartialUserDTO>(query);
    
    // Verify result
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    // Check mapped struct fields
    PartialUserDTO user = *result;
    EXPECT_EQ(2, user.id);
    EXPECT_EQ("Jane Smith", user.name);
    EXPECT_EQ(28, user.age);
}

// Test mapping multiple rows
TEST_F(DtoMappingIntegrationTest, MultipleRowMapping) {
    // Create a select query
    auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
        .from(users)
        .order_by(users.id);
    
    // Execute the query with DTO mapping for multiple rows
    auto result = conn->execute_many<UserDTO>(query);
    
    // Verify result
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    // Check result set
    const auto& users_vec = *result;
    ASSERT_EQ(5, users_vec.size());
    
    // Check first user
    EXPECT_EQ(1, users_vec[0].id);
    EXPECT_EQ("John Doe", users_vec[0].name);
    EXPECT_EQ(30, users_vec[0].age);
    
    // Check last user
    EXPECT_EQ(5, users_vec[4].id);
    EXPECT_EQ("Charlie Davis", users_vec[4].name);
    EXPECT_EQ(25, users_vec[4].age);
}

// Test mapping with field type conversion
TEST_F(DtoMappingIntegrationTest, FieldTypeConversion) {
    // Query that will need type conversion
    auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
        .from(users)
        .where(users.score > 90.0);
    
    // Get users with score > 90
    auto result = conn->execute_many<UserDTO>(query);
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    // Should return Jane and Alice
    const auto& high_scorers = *result;
    ASSERT_EQ(2, high_scorers.size());
    
    // Check conversion of numeric types
    EXPECT_TRUE(high_scorers[0].active);
    EXPECT_GT(high_scorers[0].score, 90.0);
    EXPECT_TRUE(high_scorers[1].active);
    EXPECT_GT(high_scorers[1].score, 90.0);
}

// Test filtering and conditions
TEST_F(DtoMappingIntegrationTest, FilteringAndConditions) {
    // More complex query with multiple conditions
    auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
        .from(users)
        .where((users.age > 30) && users.active)
        .order_by(users.age);
    
    // This should return Alice (active and age 42)
    auto result = conn->execute_many<UserDTO>(query);
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    const auto& filtered_users = *result;
    ASSERT_EQ(1, filtered_users.size());
    EXPECT_EQ("Alice Brown", filtered_users[0].name);
    EXPECT_EQ(42, filtered_users[0].age);
    EXPECT_TRUE(filtered_users[0].active);
}

// Test with fields in different order
// Won't be possible until we have reflection
// TEST_F(DtoMappingIntegrationTest, DifferentFieldOrder) {
//     // Query matching the field order in the DTO
//     auto query = relx::query::select(users.name, users.id, users.email, users.active, users.age, users.score)
//         .from(users)
//         .where(users.id == 3);
    
//     // Execute with the differently ordered DTO
//     auto result = conn->execute<UserDTODifferentOrder>(query);
//     ASSERT_TRUE(result) << "Failed to execute query with different field order: " << result.error().message;
    
//     // Fields should map correctly by position
//     UserDTODifferentOrder user = *result;
//     EXPECT_EQ("Bob Johnson", user.name);
//     EXPECT_EQ(3, user.id);
//     EXPECT_EQ("bob@example.com", user.email);
//     EXPECT_FALSE(user.active);
//     EXPECT_EQ(35, user.age);
//     EXPECT_DOUBLE_EQ(78.9, user.score);
// }

// Test with empty result set
TEST_F(DtoMappingIntegrationTest, EmptyResultSet) {
    // Query that will return no results
    auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
        .from(users)
        .where(users.id == 999); // Non-existent ID
    
    // This should return an error for single result
    auto single_result = conn->execute<UserDTO>(query);
    EXPECT_FALSE(single_result);
    EXPECT_EQ("No results found", single_result.error().message);
    
    // But for multiple results, it should return an empty vector
    auto multi_result = conn->execute_many<UserDTO>(query);
    ASSERT_TRUE(multi_result);
    EXPECT_TRUE(multi_result->empty());
}

} // namespace 
