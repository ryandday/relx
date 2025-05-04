#include <gtest/gtest.h>
#include <relx/postgresql.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <string>
#include <iostream>

namespace {

// Test table definition
struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<Users, "id", int, relx::identity<>> id;
    relx::schema::column<Users, "name", std::string> name;
    relx::schema::column<Users, "email", std::string> email;
    relx::schema::column<Users, "age", int> age;
    relx::schema::column<Users, "active", bool> active;
    
    relx::schema::table_primary_key<&Users::id> pk;
};

class PostgresIntegrationTest : public ::testing::Test {
protected:
    // Connection string for the PostgreSQL test database
    std::string conn_string = "host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres";
    
    void SetUp() override {
        // Clean up any existing test tables
        clean_test_table();
    }
    
    void TearDown() override {
        // Clean up after test
        clean_test_table();
    }
    
    // Helper to clean up the test table
    void clean_test_table() {
        relx::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        if (connect_result) {
            Users u;
            auto raw_sql = relx::drop_table(u).cascade();
            auto result = conn.execute(raw_sql);
            ASSERT_TRUE(result) << "Failed to drop table: " << result.error().message;
            conn.disconnect();
        }
    }
    
    // Helper to create the test table
    void create_test_table(relx::Connection& conn) {
        Users u;
        auto create_query = relx::create_table(u);
            
        auto result = conn.execute(create_query);
        ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
    }
    
    // Helper to insert test data
    void insert_test_data(relx::Connection& conn) {
        Users u;
        
        // Insert Alice
        auto insert1 = relx::query::insert_into(u)
            .columns(u.name, u.email, u.age, u.active)
            .values("Alice", "alice@example.com", 30, true);
        auto result1 = conn.execute(insert1);
        ASSERT_TRUE(result1) << "Failed to insert test data: " << result1.error().message;
        
        // Insert Bob
        auto insert2 = relx::query::insert_into(u)
            .columns(u.name, u.email, u.age, u.active)
            .values("Bob", "bob@example.com", 25, false);
        auto result2 = conn.execute(insert2);
        ASSERT_TRUE(result2) << "Failed to insert test data 2: " << result2.error().message;
        
        // Insert Charlie
        auto insert3 = relx::query::insert_into(u)
            .columns(u.name, u.email, u.age, u.active)
            .values("Charlie", "charlie@example.com", 35, true);
        auto result3 = conn.execute(insert3);
        ASSERT_TRUE(result3) << "Failed to insert test data 3: " << result3.error().message;
    }
};

TEST_F(PostgresIntegrationTest, TestBasicConnection) {
    relx::PostgreSQLConnection conn(conn_string);
    
    // Test initial state
    EXPECT_FALSE(conn.is_connected());
    
    // Test connect
    auto connect_result = conn.connect();
    ASSERT_TRUE(connect_result) << "Connect failed: " << connect_result.error().message;
    EXPECT_TRUE(conn.is_connected());
    
    // Test executing a simple query
    auto version_result = conn.execute_raw("SELECT version()");
    ASSERT_TRUE(version_result) << "Query failed: " << version_result.error().message;
    
    // Display PostgreSQL version
    const auto& version_row = (*version_result)[0];
    auto version = version_row.get<std::string>(0);
    ASSERT_TRUE(version);
    std::cout << "PostgreSQL version: " << *version << std::endl;
    
    // Disconnect
    conn.disconnect();
    EXPECT_FALSE(conn.is_connected());
}

TEST_F(PostgresIntegrationTest, TestQueryBuilderIntegration) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    insert_test_data(conn);
    
    // Create query using the query builder
    Users u;
    auto query = relx::query::select(u.id, u.name, u.email, u.age, u.active)
        .from(u)
        .where(u.age > 25)
        .order_by(u.age);
        
    // Execute the query
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Query failed: " << result.error().message;
    
    // Check result
    ASSERT_EQ(2, result->size());
    
    // Verify first row (should be Alice)
    const auto& first_row = (*result)[0];
    auto name = first_row.get<std::string>(u.name);
    ASSERT_TRUE(name);
    EXPECT_EQ("Alice", *name);
    
    auto age = first_row.get<int>(u.age);
    ASSERT_TRUE(age);
    EXPECT_EQ(30, *age);
    
    // Verify second row (should be Charlie)
    const auto& second_row = (*result)[1];
    auto name2 = second_row.get<std::string>(u.name);
    ASSERT_TRUE(name2);
    EXPECT_EQ("Charlie", *name2);
    
    auto age2 = second_row.get<int>(u.age);
    ASSERT_TRUE(age2);
    EXPECT_EQ(35, *age2);
    
    // Disconnect
    conn.disconnect();
}

TEST_F(PostgresIntegrationTest, TestParameterizedQueries) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    insert_test_data(conn);
    
    // Create a parameterized query
    Users u;
    auto query = relx::query::select(u.id, u.name, u.email)
        .from(u)
        .where(u.active == true && u.age > 30);
        
    // Execute the query
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Query failed: " << result.error().message;
    
    // Check result
    ASSERT_EQ(1, result->size());
    
    // Verify that only Charlie is returned
    const auto& row = (*result)[0];
    auto name = row.get<std::string>(u.name);
    ASSERT_TRUE(name);
    EXPECT_EQ("Charlie", *name);
    
    // Test a more complex query with multiple conditions
    auto complex_query = relx::query::select(u.id, u.name)
        .from(u)
        .where(u.age > 20 && (u.active == true || u.name == "Bob"))
        .order_by(u.name);
        
    auto complex_result = conn.execute(complex_query);
    ASSERT_TRUE(complex_result) << "Complex query failed: " << complex_result.error().message;
    
    // Should return all three users (Alice, Bob, Charlie) in alphabetical order
    ASSERT_EQ(3, complex_result->size());
    
    // Verify order: Alice, Bob, Charlie
    EXPECT_EQ("Alice", *(*complex_result)[0].get<std::string>(u.name));
    EXPECT_EQ("Bob", *(*complex_result)[1].get<std::string>(u.name));
    EXPECT_EQ("Charlie", *(*complex_result)[2].get<std::string>(u.name));
    
    // Disconnect
    conn.disconnect();
}

} // namespace 