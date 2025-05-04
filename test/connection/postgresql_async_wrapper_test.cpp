#include <gtest/gtest.h>

#include <relx/connection/pgsql_async_wrapper.hpp>
#include <thread>
#include <future>
#include <chrono>

using namespace relx::pgsql_async_wrapper;
namespace asio = boost::asio;

class PostgresqlAsyncWrapperTest : public ::testing::Test {
protected:
    asio::io_context io_;
    
    // Connection string for the test database - matching other test files
    std::string conn_string = "host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres";
    
    void SetUp() override {
        // Nothing to do here yet
    }

    void TearDown() override {
        io_.stop();
    }

    void run_test(std::function<asio::awaitable<void>()> test_coro) {
        asio::co_spawn(io_, test_coro, asio::detached);
        io_.run();
    }


};

// Test exception classes construction and inheritance
TEST_F(PostgresqlAsyncWrapperTest, ExceptionClasses) {
    // Test pg_error
    try {
        throw pg_error("Test error");
    } catch (const pg_error& e) {
        EXPECT_STREQ("Test error", e.what());
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }

    // Test connection_error
    try {
        throw connection_error("Connection error");
    } catch (const pg_error& e) {
        EXPECT_STREQ("Connection error", e.what());
        EXPECT_TRUE(dynamic_cast<const connection_error*>(&e) != nullptr);
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }

    // Test query_error
    try {
        throw query_error("Query error");
    } catch (const pg_error& e) {
        EXPECT_STREQ("Query error", e.what());
        EXPECT_TRUE(dynamic_cast<const query_error*>(&e) != nullptr);
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }
}

// Test result class methods with nullptr
TEST_F(PostgresqlAsyncWrapperTest, ResultNullptr) {
    result res;
    
    EXPECT_FALSE(res.ok());
    EXPECT_EQ(PGRES_FATAL_ERROR, res.status());
    EXPECT_STREQ("No result available", res.error_message());
    EXPECT_EQ(0, res.rows());
    EXPECT_EQ(0, res.columns());
    EXPECT_EQ(nullptr, res.field_name(0));
    EXPECT_EQ(0, res.field_type(0));
    EXPECT_EQ(0, res.field_size(0));
    EXPECT_EQ(-1, res.field_number("column"));
    EXPECT_TRUE(res.is_null(0, 0));
    EXPECT_EQ(nullptr, res.get_value(0, 0));
    EXPECT_EQ(0, res.get_length(0, 0));
    EXPECT_EQ(nullptr, res.get());
    EXPECT_FALSE(res);
}

// Test connection_error with invalid connection parameters
TEST_F(PostgresqlAsyncWrapperTest, ConnectionErrorInvalidParams) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect("this is not a valid connection string");
        EXPECT_FALSE(conn.is_open());
    });
}

// Test connection_error when server is offline
TEST_F(PostgresqlAsyncWrapperTest, ConnectionErrorServerOffline) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        // Use a non-existent port to simulate server being offline
        co_await conn.connect("host=localhost port=54321 dbname=nonexistent user=postgres password=postgres");
        EXPECT_FALSE(conn.is_open());
    });
}

// Test using socket before initialization
TEST_F(PostgresqlAsyncWrapperTest, SocketNotInitialized) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        conn.socket();
    });
}

// Test query on closed connection
TEST_F(PostgresqlAsyncWrapperTest, QueryOnClosedConnection) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        auto res = co_await conn.query("SELECT 1", {});
        EXPECT_FALSE(res.ok());
    });
}

// Test move constructor and assignment
TEST_F(PostgresqlAsyncWrapperTest, MoveOperations) {
    // Move constructor
    {
        connection conn1(io_);
        connection conn2(std::move(conn1));
        
        // conn1 should be in a moved-from state
        EXPECT_FALSE(conn1.is_open());
        
        // conn2 should be properly initialized but not connected
        EXPECT_FALSE(conn2.is_open());
    }
    
    // Move assignment
    {
        connection conn1(io_);
        connection conn2(io_);
        
        conn2 = std::move(conn1);
        
        // conn1 should be in a moved-from state
        EXPECT_FALSE(conn1.is_open());
        
        // conn2 should be properly initialized but not connected
        EXPECT_FALSE(conn2.is_open());
    }
}

// Test result move operations
TEST_F(PostgresqlAsyncWrapperTest, ResultMoveOperations) {
    // Move constructor
    {
        result res1;
        result res2(std::move(res1));
        
        EXPECT_FALSE(res2.ok());
    }
    
    // Move assignment
    {
        result res1;
        result res2;
        
        res2 = std::move(res1);
        
        EXPECT_FALSE(res2.ok());
    }
}

// Tests requiring actual PostgreSQL server
// These will be run when a PostgreSQL server is available

// Test successful connection to real PostgreSQL server
TEST_F(PostgresqlAsyncWrapperTest, RealConnectionSuccess) {
    connection conn(io_);
    
    // Will throw exception if connection fails 
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Verify connection with simple query
        result res = co_await conn.query("SELECT 1 as num", {});
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_EQ(1, res.columns());
        EXPECT_STREQ("num", res.field_name(0));
        EXPECT_STREQ("1", res.get_value(0, 0));
    });
}

// Test malformed SQL query (only runs if real PostgreSQL is available)
TEST_F(PostgresqlAsyncWrapperTest, MalformedQuery) {
    connection conn(io_);
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Test a malformed query
        co_await conn.query("SELECT FROM WHERE", {});
    });
}

// Test parameterized query (only runs if real PostgreSQL is available)
TEST_F(PostgresqlAsyncWrapperTest, ParameterizedQuery) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        
        EXPECT_TRUE(conn.is_open());
        
        // Test a parameterized query
        result res = co_await conn.query("SELECT $1::int as num", {"42"});
        
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_EQ(1, res.columns());
        EXPECT_STREQ("num", res.field_name(0));
        EXPECT_STREQ("42", res.get_value(0, 0));
    });
}

// Test connection close and reconnect (only runs if real PostgreSQL is available)
TEST_F(PostgresqlAsyncWrapperTest, ConnectionCloseAndReconnect) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Close the connection
        conn.close();
        EXPECT_FALSE(conn.is_open());
        
        // Reconnect
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Test with a simple query after reconnect
        result res = co_await conn.query("SELECT 1", {});
        EXPECT_TRUE(res.ok());
    });
}
