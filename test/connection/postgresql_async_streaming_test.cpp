#include <gtest/gtest.h>
#include "relx/connection/postgresql_async_streaming_source.hpp"
#include "relx/connection/postgresql_async_connection.hpp"
#include "relx/results/lazy_result.hpp"
#include "relx/schema.hpp"
#include "relx/query.hpp"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace relx;
namespace asio = boost::asio;

// Test fixture for PostgreSQL async streaming tests
class PostgreSQLAsyncStreamingTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    
    // Connection string for the test database - matching other test files
    std::string conn_string = "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
    
    void SetUp() override {
        // Nothing to do here yet
    }

    void TearDown() override {
        io_context.stop();
    }

    void run_test(std::function<asio::awaitable<void>()> test_coro) {
        asio::co_spawn(io_context, test_coro, asio::detached);
        io_context.run();
    }
};

// Test basic async streaming functionality
TEST_F(PostgreSQLAsyncStreamingTest, BasicAsyncStreamingFunctionality) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table and data
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                email VARCHAR(100),
                age INTEGER NOT NULL
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM users");
        EXPECT_TRUE(clear_result);

        auto insert_result = co_await conn.execute_raw(R"(
            INSERT INTO users (name, email, age) VALUES 
            ('Alice Johnson', 'alice@example.com', 30),
            ('Bob Smith', 'bob@example.com', 25),
            ('Charlie Brown', NULL, 35)
        )");
        EXPECT_TRUE(insert_result);

        // Test async streaming
        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT id, name, email, age FROM users ORDER BY id");

                 std::vector<std::string> names;
         co_await streaming_result.for_each([&names](const auto& lazy_row) {
             auto name_result = lazy_row.template get<std::string>("name");
             if (name_result) {
                 names.push_back(*name_result);
             }
         });

        EXPECT_GT(names.size(), 0);
        if (!names.empty()) {
            EXPECT_EQ(names[0], "Alice Johnson");
        }
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS users");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
}

// Test async streaming with parameters
TEST_F(PostgreSQLAsyncStreamingTest, AsyncStreamingWithParameters) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table and data
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                email VARCHAR(100),
                age INTEGER NOT NULL
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM users");
        EXPECT_TRUE(clear_result);

        auto insert_result = co_await conn.execute_raw(R"(
            INSERT INTO users (name, email, age) VALUES 
            ('Alice Johnson', 'alice@example.com', 30),
            ('Bob Smith', 'bob@example.com', 25),
            ('Charlie Brown', NULL, 35),
            ('Eve Wilson', 'eve@example.com', 32)
        )");
        EXPECT_TRUE(insert_result);

        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT id, name, age FROM users WHERE age > $1 ORDER BY age", 30);

                 std::vector<int> ages;
         co_await streaming_result.for_each([&ages](const auto& lazy_row) {
             auto age_result = lazy_row.template get<int>("age");
             if (age_result) {
                 ages.push_back(*age_result);
             }
         });

        EXPECT_GT(ages.size(), 0);  // Should get some results
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS users");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
}

// Test async streaming with NULL values
TEST_F(PostgreSQLAsyncStreamingTest, AsyncStreamingWithNullValues) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table and data
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                email VARCHAR(100),
                age INTEGER NOT NULL
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM users");
        EXPECT_TRUE(clear_result);

        auto insert_result = co_await conn.execute_raw(R"(
            INSERT INTO users (name, email, age) VALUES 
            ('Charlie Brown', NULL, 35)
        )");
        EXPECT_TRUE(insert_result);

        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT name, email FROM users WHERE email IS NULL");

                 std::vector<std::string> names_with_null_email;
         co_await streaming_result.for_each([&names_with_null_email](const auto& lazy_row) {
             auto name_result = lazy_row.template get<std::string>("name");
             auto email_result = lazy_row.template get<std::optional<std::string>>("email");
             
             if (name_result && email_result && !email_result->has_value()) {
                 names_with_null_email.push_back(*name_result);
             }
         });

        EXPECT_GT(names_with_null_email.size(), 0);
        if (!names_with_null_email.empty()) {
            EXPECT_EQ(names_with_null_email[0], "Charlie Brown");
        }
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS users");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
}

// Test manual async iteration
TEST_F(PostgreSQLAsyncStreamingTest, ManualAsyncIteration) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table and data
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                email VARCHAR(100),
                age INTEGER NOT NULL
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM users");
        EXPECT_TRUE(clear_result);

        auto insert_result = co_await conn.execute_raw(R"(
            INSERT INTO users (name, email, age) VALUES 
            ('Alice Johnson', 'alice@example.com', 30),
            ('Bob Smith', 'bob@example.com', 25),
            ('Charlie Brown', NULL, 35)
        )");
        EXPECT_TRUE(insert_result);

        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT id, name FROM users ORDER BY id LIMIT 3");

        auto it = streaming_result.begin();
        std::vector<std::pair<int, std::string>> results;

        // Manually iterate through results
        co_await it.advance();
        while (!it.is_at_end()) {
            const auto& lazy_row = *it;
            
                         auto id_result = lazy_row.template get<int>("id");
             auto name_result = lazy_row.template get<std::string>("name");
            
            if (id_result && name_result) {
                results.emplace_back(*id_result, *name_result);
            }
            
            co_await it.advance();
        }

        EXPECT_GT(results.size(), 0);  // Should get at least one result
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS users");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
}

// Test basic async streaming setup and teardown
TEST_F(PostgreSQLAsyncStreamingTest, BasicStreamingSetup) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Simple test - just verify we can create streaming result
        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT 1 as num");

        // Verify we can iterate (even if empty)
                 std::vector<int> results;
         co_await streaming_result.for_each([&results](const auto& lazy_row) {
             auto num_result = lazy_row.template get<int>("num");
             if (num_result) {
                 results.push_back(*num_result);
             }
         });

        // Just verify no crash occurred
        EXPECT_TRUE(true);
        
        co_await conn.disconnect();
    });
}

// Test async streaming with empty result set
TEST_F(PostgreSQLAsyncStreamingTest, AsyncStreamingEmptyResultSet) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table but don't insert any data
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS empty_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM empty_test");
        EXPECT_TRUE(clear_result);

        // Query empty table
        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT id, name FROM empty_test");

        std::vector<std::string> names;
        co_await streaming_result.for_each([&names](const auto& lazy_row) {
            auto name_result = lazy_row.template get<std::string>("name");
            if (name_result) {
                names.push_back(*name_result);
            }
        });

        EXPECT_EQ(names.size(), 0);  // Should be empty
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS empty_test");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
}

// Test async streaming with different data types
TEST_F(PostgreSQLAsyncStreamingTest, AsyncStreamingMixedDataTypes) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table with various data types
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS mixed_types_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                price DECIMAL(10,2),
                is_active BOOLEAN,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM mixed_types_test");
        EXPECT_TRUE(clear_result);

        auto insert_result = co_await conn.execute_raw(R"(
            INSERT INTO mixed_types_test (name, price, is_active) VALUES 
            ('Product A', 29.99, true),
            ('Product B', 15.50, false),
            ('Product C', 99.95, true)
        )");
        EXPECT_TRUE(insert_result);

        // Test streaming with mixed data types
        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT name, price, is_active FROM mixed_types_test ORDER BY name");

        std::vector<std::tuple<std::string, std::string, std::string>> results;
        co_await streaming_result.for_each([&results](const auto& lazy_row) {
            auto name_result = lazy_row.template get<std::string>("name");
            auto price_result = lazy_row.template get<std::string>("price");  // Get as string to avoid decimal parsing complexity
            auto active_result = lazy_row.template get<std::string>("is_active");
            
            if (name_result && price_result && active_result) {
                results.emplace_back(*name_result, *price_result, *active_result);
            }
        });

        EXPECT_GT(results.size(), 0);  // Should get some results
        if (!results.empty()) {
            EXPECT_EQ(std::get<0>(results[0]), "Product A");
        }
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS mixed_types_test");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
}

// Test async streaming error handling
TEST_F(PostgreSQLAsyncStreamingTest, AsyncStreamingErrorHandling) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Test with invalid SQL
        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT * FROM nonexistent_table_12345");

        std::vector<std::string> results;
        co_await streaming_result.for_each([&results](const auto& lazy_row) {
            // This should not be called due to the SQL error
            results.push_back("should_not_reach_here");
        });

        // Should get no results due to error
        EXPECT_EQ(results.size(), 0);
        
        co_await conn.disconnect();
    });
}

// Test async streaming with large parameter list
TEST_F(PostgreSQLAsyncStreamingTest, AsyncStreamingWithMultipleParameters) {
    connection::PostgreSQLAsyncConnection conn(io_context, conn_string);
    
    run_test([&]() -> asio::awaitable<void> {
        auto connect_result = co_await conn.connect();
        EXPECT_TRUE(connect_result);
        
        // Create test table
        auto create_result = co_await conn.execute_raw(R"(
            CREATE TABLE IF NOT EXISTS param_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                category VARCHAR(50),
                price DECIMAL(10,2),
                in_stock BOOLEAN
            )
        )");
        EXPECT_TRUE(create_result);

        auto clear_result = co_await conn.execute_raw("DELETE FROM param_test");
        EXPECT_TRUE(clear_result);

        auto insert_result = co_await conn.execute_raw(R"(
            INSERT INTO param_test (name, category, price, in_stock) VALUES 
            ('Widget A', 'electronics', 25.99, true),
            ('Widget B', 'electronics', 35.50, false),
            ('Gadget C', 'tools', 15.75, true),
            ('Gadget D', 'tools', 45.00, true),
            ('Item E', 'misc', 5.25, false)
        )");
        EXPECT_TRUE(insert_result);

        // Test streaming with multiple parameters
        auto streaming_result = connection::create_async_streaming_result(
            conn, "SELECT name, price FROM param_test WHERE category = $1 AND price > $2 AND in_stock = $3 ORDER BY price",
            "electronics", 20.0, true);

        std::vector<std::pair<std::string, std::string>> results;
        co_await streaming_result.for_each([&results](const auto& lazy_row) {
            auto name_result = lazy_row.template get<std::string>("name");
            auto price_result = lazy_row.template get<std::string>("price");
            
            if (name_result && price_result) {
                results.emplace_back(*name_result, *price_result);
            }
        });

        EXPECT_GT(results.size(), 0);  // Should get some results
        if (!results.empty()) {
            EXPECT_EQ(results[0].first, "Widget A");  // Should match our criteria
        }
        
        // Clean up
        auto drop_result = co_await conn.execute_raw("DROP TABLE IF EXISTS param_test");
        EXPECT_TRUE(drop_result);
        
        co_await conn.disconnect();
    });
} 