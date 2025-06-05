#include <gtest/gtest.h>

#include <relx/connection/postgresql_connection_pool.hpp>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <future>
#include <expected>

namespace {

class PostgreSQLConnectionPoolTest : public ::testing::Test {
protected:
    // Connection string for the Docker container
    std::string conn_string = "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
    
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
        relx::connection::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        if (connect_result) {
            // Drop table if it exists
            conn.execute_raw("DROP TABLE IF EXISTS connection_pool_test");
            conn.disconnect();
        }
    }
    
    // Helper to create the test table using a connection from the pool
    void create_test_table(relx::connection::PostgreSQLConnectionPool::PooledConnection& conn) {
        std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS connection_pool_test (
                id SERIAL PRIMARY KEY,
                thread_id INTEGER NOT NULL,
                value INTEGER NOT NULL
            )
        )";
        auto result = conn->execute_raw(create_table_sql);
        ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
    }
    
    // Helper to create the test table with a direct connection
    void create_test_table(relx::connection::PostgreSQLConnection& conn) {
        std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS connection_pool_test (
                id SERIAL PRIMARY KEY,
                thread_id INTEGER NOT NULL,
                value INTEGER NOT NULL
            )
        )";
        auto result = conn.execute_raw(create_table_sql);
        ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
    }
};

TEST_F(PostgreSQLConnectionPoolTest, TestPoolInitialization) {
    relx::connection::PostgreSQLConnectionPoolConfig config;
    config.connection_params = {
        .host = "localhost",
        .port = 5434,
        .dbname = "relx_test",
        .user = "postgres",
        .password = "postgres"
    };
    config.initial_size = 3;
    config.max_size = 5;
    
    auto pool = relx::connection::PostgreSQLConnectionPool::create(config);
    auto init_result = pool->initialize();
    
    ASSERT_TRUE(init_result) << "Failed to initialize pool: " << init_result.error().message;
    
    // Check initial pool state
    EXPECT_EQ(0, pool->active_connections());
    EXPECT_EQ(3, pool->idle_connections());
    
    {
        // Get a connection in a new scope so it's returned when the scope ends
        auto conn_result = pool->get_connection();
        ASSERT_TRUE(conn_result) << "Failed to get connection: " << conn_result.error().message;
        
        // Check pool state after getting a connection
        EXPECT_EQ(1, pool->active_connections());
        EXPECT_EQ(2, pool->idle_connections());
        
        // Connection will be automatically returned when it goes out of scope
    }
    
    // Check pool state after connection is returned via destructor
    EXPECT_EQ(0, pool->active_connections());
    EXPECT_EQ(3, pool->idle_connections());
}

TEST_F(PostgreSQLConnectionPoolTest, TestPoolMaxConnections) {
    relx::connection::PostgreSQLConnectionPoolConfig config;
    config.connection_params = {
        .host = "localhost",
        .port = 5434,
        .dbname = "relx_test",
        .user = "postgres",
        .password = "postgres"
    };
    config.initial_size = 2;
    config.max_size = 4;
    config.connection_timeout = std::chrono::milliseconds(500); // Short timeout for testing
    
    auto pool = relx::connection::PostgreSQLConnectionPool::create(config);
    ASSERT_TRUE(pool->initialize());
    
    // Get all connections from the pool
    std::vector<std::optional<relx::connection::PostgreSQLConnectionPool::PooledConnection>> connections;
    connections.reserve(4);
    
    for (int i = 0; i < 4; ++i) {
        auto conn_result = pool->get_connection();
        ASSERT_TRUE(conn_result) << "Failed to get connection " << i << ": " << conn_result.error().message;
        connections.emplace_back(std::move(*conn_result));
    }
    
    // Pool should now be at max capacity
    EXPECT_EQ(4, pool->active_connections());
    EXPECT_EQ(0, pool->idle_connections());
    
    // Trying to get another connection should fail with timeout
    auto conn_result = pool->get_connection();
    EXPECT_FALSE(conn_result);
    EXPECT_NE("", conn_result.error().message);
    
    // Return one connection
    connections[3].reset(); // This will trigger auto-return via destructor
    
    // Now we should be able to get a connection again
    conn_result = pool->get_connection();
    ASSERT_TRUE(conn_result) << "Failed to get connection after returning one: " << conn_result.error().message;
    connections[3].emplace(std::move(*conn_result));
    
    // Connections will be automatically returned when they go out of scope
}

TEST_F(PostgreSQLConnectionPoolTest, TestPoolWithConnection) {
    relx::connection::PostgreSQLConnectionPoolConfig config;
    config.connection_params = {
        .host = "localhost",
        .port = 5434,
        .dbname = "relx_test",
        .user = "postgres",
        .password = "postgres"
    };
    config.initial_size = 2;
    config.max_size = 5;
    
    auto pool = relx::connection::PostgreSQLConnectionPool::create(config);
    ASSERT_TRUE(pool->initialize());
    
    // Create test table using the pool
    auto create_result = pool->with_connection([this](auto& conn) -> relx::connection::ConnectionResult<void> {
        create_test_table(conn);
        return {};
    });
    
    ASSERT_TRUE(create_result) << "Failed to create table: " << create_result.error().message;
    
    // Insert some data using the pool
    auto insert_result = pool->with_connection([](auto& conn) -> relx::connection::ConnectionResult<int> {
        auto result = conn->execute_raw(
            "INSERT INTO connection_pool_test (thread_id, value) VALUES ($1, $2) RETURNING id",
            {"0", "42"}
        );
        
        if (!result) {
            return std::unexpected(result.error());
        }
        
        // Get the returned ID
        const auto& row = (*result)[0];
        auto id = row.template get<int>("id");
        if (!id) {
            return std::unexpected(relx::connection::ConnectionError{
                .message = "Failed to get returned ID",
                .error_code = -1
            });
        }
        
        return *id;
    });
    
    ASSERT_TRUE(insert_result) << "Failed to insert data: " << insert_result.error().message;
    int result = **insert_result; // TODO why do I get linker errors when I use .value()?
    EXPECT_GT(result, 0);
    
    // Check that the data was inserted
    auto check_result = pool->with_connection([](auto& conn) -> relx::connection::ConnectionResult<int> {
        auto result = conn->execute_raw("SELECT COUNT(*) FROM connection_pool_test");
        
        if (!result) {
            return std::unexpected(result.error());
        }
        
        const auto& row = (*result)[0];
        auto count = row.template get<int>(0);
        if (!count) {
            return std::unexpected(relx::connection::ConnectionError{
                .message = "Failed to get count",
                .error_code = -1
            });
        }
        
        return *count;
    });
    
    ASSERT_TRUE(check_result) << "Failed to check data: " << check_result.error().message;
    EXPECT_EQ(1, *check_result);
}

TEST_F(PostgreSQLConnectionPoolTest, TestPoolMultithreaded) {
    relx::connection::PostgreSQLConnectionPoolConfig config;
    config.connection_params = {
        .host = "localhost",
        .port = 5434,
        .dbname = "relx_test",
        .user = "postgres",
        .password = "postgres"
    };
    config.initial_size = 3;
    config.max_size = 10;
    
    auto pool = relx::connection::PostgreSQLConnectionPool::create(config);
    ASSERT_TRUE(pool->initialize());
    
    // Create test table
    auto create_result = pool->with_connection([this](auto& conn) {
        create_test_table(conn);
        return true;
    });
    ASSERT_TRUE(create_result) << "Failed to create table";
    
    // Run multiple threads to test the pool
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 5;
    std::atomic<int> success_count = 0;
    
    auto thread_func = [&pool, &success_count](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i) {
            auto result = pool->with_connection([thread_id, i](auto& conn) -> relx::connection::ConnectionResult<void> {
                auto insert_result = conn->execute_raw(
                    "INSERT INTO connection_pool_test (thread_id, value) VALUES ($1, $2)",
                    {std::to_string(thread_id), std::to_string(i)}
                );
                
                if (!insert_result) {
                    return std::unexpected(insert_result.error());
                }
                
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                return {};
            });
            
            if (result) {
                success_count++;
            }
        }
    };
    
    // Launch threads
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, thread_func, i));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Check that all operations succeeded
    EXPECT_EQ(num_threads * operations_per_thread, success_count);
    
    // Verify the data
    auto count_result = pool->with_connection([](auto& conn) -> relx::connection::ConnectionResult<int> {
        auto result = conn->execute_raw("SELECT COUNT(*) FROM connection_pool_test");
        
        if (!result) {
            return std::unexpected(result.error());
        }
        
        const auto& row = (*result)[0];
        auto count = row.template get<int>(0);
        if (!count) {
            return std::unexpected(relx::connection::ConnectionError{
                .message = "Failed to get count",
                .error_code = -1
            });
        }
        
        return *count;
    });
    
    ASSERT_TRUE(count_result) << "Failed to count data: " << count_result.error().message;
    EXPECT_EQ(num_threads * operations_per_thread, *count_result);
}

TEST_F(PostgreSQLConnectionPoolTest, TestPoolConnectionValidation) {
    relx::connection::PostgreSQLConnectionPoolConfig config;
    config.connection_params = {
        .host = "localhost",
        .port = 5434,
        .dbname = "relx_test",
        .user = "postgres",
        .password = "postgres"
    };
    config.initial_size = 2;
    config.max_size = 4;
    config.validate_connections = true;
    
    auto pool = relx::connection::PostgreSQLConnectionPool::create(config);
    ASSERT_TRUE(pool->initialize());
    
    // Check initial state
    EXPECT_EQ(0, pool->active_connections());
    EXPECT_EQ(2, pool->idle_connections());
    
    {
        // Get a connection and manually disconnect it to make it invalid
        auto conn_result = pool->get_connection();
        ASSERT_TRUE(conn_result) << "Failed to get connection: " << conn_result.error().message;
        
        // Manually disconnect this connection to make it invalid
        (*conn_result)->disconnect();
        
        // Connection will be automatically returned when it goes out of scope
    }
    
    // The invalid connection should be discarded from the pool when it's returned
    EXPECT_EQ(0, pool->active_connections());
    EXPECT_EQ(1, pool->idle_connections());  // Only one valid connection remains
    
    {
        // Get another connection - should still work
        auto conn_result = pool->get_connection();
        ASSERT_TRUE(conn_result) << "Failed to get replacement connection: " << conn_result.error().message;
        EXPECT_TRUE((*conn_result)->is_connected());
        
        // Connection will be automatically returned when it goes out of scope
    }
    
    // Connection should be back in the pool
    EXPECT_EQ(0, pool->active_connections());
    EXPECT_EQ(1, pool->idle_connections());
}

} // namespace 