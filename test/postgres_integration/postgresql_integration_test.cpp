#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>
#include <relx/postgresql.hpp>
#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <iostream>

namespace {

// Test configuration (could be loaded from environment variables in production)
const std::string POSTGRES_CONNECTION = "host=localhost port=5432 dbname=relx_test user=postgres password=postgres";

// Base test fixture for PostgreSQL integration tests
class PostgreSQLIntegrationTest : public ::testing::Test {
protected:
    using Connection = relx::connection::PostgreSQLConnection;
    
    std::unique_ptr<Connection> conn;
    
    void SetUp() override {
        conn = std::make_unique<Connection>(POSTGRES_CONNECTION);
        auto result = conn->connect();
        ASSERT_TRUE(result) << "Failed to connect: " << result.error().message;
        
        // Start with a clean database - drop tables if they exist
        cleanup_database();
    }
    
    void TearDown() override {
        if (conn && conn->is_connected()) {
            cleanup_database();
            conn->disconnect();
        }
    }
    
    // Helper to drop all test tables
    void cleanup_database() {
        // We'll define tables in each test file and drop them here
        // This is just a starting point - add more tables as needed
        auto result = conn->execute_raw("DROP TABLE IF EXISTS orders CASCADE");
        ASSERT_TRUE(result) << "Failed to drop orders table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS customers CASCADE");
        ASSERT_TRUE(result) << "Failed to drop customers table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS products CASCADE");
        ASSERT_TRUE(result) << "Failed to drop products table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS categories CASCADE");
        ASSERT_TRUE(result) << "Failed to drop categories table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS inventory CASCADE");
        ASSERT_TRUE(result) << "Failed to drop inventory table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS users CASCADE");
        ASSERT_TRUE(result) << "Failed to drop users table: " << result.error().message;
    }
};

// Simple test to verify connectivity
TEST_F(PostgreSQLIntegrationTest, ConnectionWorks) {
    ASSERT_TRUE(conn->is_connected());
    
    // Simple query to verify database is responsive
    auto result = conn->execute_raw("SELECT 1 as test");
    ASSERT_TRUE(result) << "Failed to execute test query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(1, rows.size());
    
    auto val = rows[0].get<int>(0);
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(1, *val);
}

} // namespace 