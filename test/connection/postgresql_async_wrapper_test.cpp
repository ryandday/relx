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
    std::string conn_string = "host=localhost port=5434 dbname=sqllib_Test user=postgres password=postgres";
    
    void SetUp() override {
        // Nothing to do here yet
    }

    void TearDown() override {
        io_.stop();
    }

    // Helper to run an awaitable function in the io_context
    template<typename Awaitable>
    auto run_awaitable(Awaitable awaitable) -> typename Awaitable::value_type {
        using result_type = typename Awaitable::value_type;
        
        std::promise<result_type> promise;
        std::future<result_type> future = promise.get_future();

        asio::co_spawn(io_, 
            [awaitable = std::move(awaitable)]() mutable -> Awaitable {
                return std::move(awaitable);
            },
            [&promise](std::exception_ptr e, result_type result) {
                if (e) {
                    promise.set_exception(e);
                } else {
                    promise.set_value(std::move(result));
                }
            });

        // Run the io_context in this thread instead of joining another thread
        // Use a work guard to keep io_context alive until we're done
        asio::executor_work_guard<asio::io_context::executor_type> work_guard = 
            asio::make_work_guard(io_);
            
        // Run the io_context until the future is ready or timeout occurs
        while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
            io_.poll();
        }
        
        // Allow the io_context to finish naturally
        work_guard.reset();
        io_.run_one();
        
        return future.get(); // This will rethrow any exceptions
    }

    // Specialization for void return type
    auto run_awaitable_void(asio::awaitable<void> awaitable) -> void {
        std::promise<void> promise;
        std::future<void> future = promise.get_future();

        asio::co_spawn(io_, 
            [awaitable = std::move(awaitable)]() mutable -> asio::awaitable<void> {
                return std::move(awaitable);
            },
            [&promise](std::exception_ptr e) {
                if (e) {
                    promise.set_exception(e);
                } else {
                    promise.set_value();
                }
            });

        // Run the io_context in this thread instead of joining another thread
        // Use a work guard to keep io_context alive until we're done
        asio::executor_work_guard<asio::io_context::executor_type> work_guard = 
            asio::make_work_guard(io_);
            
        // Run the io_context until the future is ready or timeout occurs
        while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
            io_.poll();
        }
        
        // Allow the io_context to finish naturally
        work_guard.reset();
        io_.run_one();
        
        future.get(); // This will rethrow any exceptions
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
    
    EXPECT_THROW({
        run_awaitable_void(conn.connect("this is not a valid connection string"));
    }, connection_error);
}

// Test connection_error when server is offline
TEST_F(PostgresqlAsyncWrapperTest, ConnectionErrorServerOffline) {
    connection conn(io_);
    
    EXPECT_THROW({
        // Use a non-existent port to simulate server being offline
        run_awaitable_void(conn.connect("host=localhost port=54321 dbname=nonexistent user=postgres password=postgres"));
    }, connection_error);
}

// Test using socket before initialization
TEST_F(PostgresqlAsyncWrapperTest, SocketNotInitialized) {
    connection conn(io_);
    
    EXPECT_THROW({
        conn.socket();
    }, connection_error);
}

// Test query on closed connection
TEST_F(PostgresqlAsyncWrapperTest, QueryOnClosedConnection) {
    connection conn(io_);
    
    EXPECT_THROW({
        run_awaitable(conn.query("SELECT 1", {}));
    }, connection_error);
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
    try {
        run_awaitable_void(conn.connect(conn_string));
        EXPECT_TRUE(conn.is_open());
        
        // Verify connection with simple query
        result res = run_awaitable(conn.query("SELECT 1 as num", {}));
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_EQ(1, res.columns());
        EXPECT_STREQ("num", res.field_name(0));
        EXPECT_STREQ("1", res.get_value(0, 0));
    } catch (const connection_error& e) {
        std::cout << "Skipping test: Failed to connect to PostgreSQL server: " << e.what() << std::endl;
    }
}

// Test malformed SQL query (only runs if real PostgreSQL is available)
TEST_F(PostgresqlAsyncWrapperTest, MalformedQuery) {
    connection conn(io_);
    
    try {
        run_awaitable_void(conn.connect(conn_string));
        
        EXPECT_TRUE(conn.is_open());
        
        // Test a malformed query
        EXPECT_THROW({
            run_awaitable(conn.query("SELECT FROM WHERE", {}));
        }, query_error);
    } catch (const connection_error& e) {
        std::cout << "Skipping test: Failed to connect to PostgreSQL server: " << e.what() << std::endl;
    }
}

// Test parameterized query (only runs if real PostgreSQL is available)
TEST_F(PostgresqlAsyncWrapperTest, ParameterizedQuery) {
    connection conn(io_);
    
    try {
        run_awaitable_void(conn.connect(conn_string));
        
        EXPECT_TRUE(conn.is_open());
        
        // Test a parameterized query
        result res = run_awaitable(conn.query("SELECT $1::int as num", {"42"}));
        
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_EQ(1, res.columns());
        EXPECT_STREQ("num", res.field_name(0));
        EXPECT_STREQ("42", res.get_value(0, 0));
    } catch (const connection_error& e) {
        std::cout << "Skipping test: Failed to connect to PostgreSQL server: " << e.what() << std::endl;
    }
}

// Test connection close and reconnect (only runs if real PostgreSQL is available)
TEST_F(PostgresqlAsyncWrapperTest, ConnectionCloseAndReconnect) {
    connection conn(io_);
    
    try {
        run_awaitable_void(conn.connect(conn_string));
        EXPECT_TRUE(conn.is_open());
        
        // Close the connection
        conn.close();
        EXPECT_FALSE(conn.is_open());
        
        // Reconnect
        run_awaitable_void(conn.connect(conn_string));
        EXPECT_TRUE(conn.is_open());
        
        // Test with a simple query after reconnect
        result res = run_awaitable(conn.query("SELECT 1", {}));
        EXPECT_TRUE(res.ok());
    } catch (const connection_error& e) {
        std::cout << "Skipping test: Failed to connect to PostgreSQL server: " << e.what() << std::endl;
    }
}
