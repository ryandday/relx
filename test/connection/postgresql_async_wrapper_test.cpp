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
    
    // Test transaction_error
    try {
        throw transaction_error("Transaction error");
    } catch (const pg_error& e) {
        EXPECT_STREQ("Transaction error", e.what());
        EXPECT_TRUE(dynamic_cast<const transaction_error*>(&e) != nullptr);
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }
    
    // Test statement_error
    try {
        throw statement_error("Statement error");
    } catch (const pg_error& e) {
        EXPECT_STREQ("Statement error", e.what());
        EXPECT_TRUE(dynamic_cast<const statement_error*>(&e) != nullptr);
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

// Transaction Tests

// Test basic transaction operations (begin, commit)
TEST_F(PostgresqlAsyncWrapperTest, BasicTransaction) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        EXPECT_FALSE(conn.in_transaction());
        
        // Begin a transaction
        co_await conn.begin_transaction();
        EXPECT_TRUE(conn.in_transaction());
        
        // Create a temporary table for testing
        result res = co_await conn.query("CREATE TEMP TABLE transaction_test (id INT, name TEXT)");
        EXPECT_TRUE(res.ok());
        
        // Insert data
        res = co_await conn.query("INSERT INTO transaction_test VALUES (1, 'Test 1')");
        EXPECT_TRUE(res.ok());
        
        // Verify data exists within transaction
        res = co_await conn.query("SELECT * FROM transaction_test");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        
        // Commit the transaction
        co_await conn.commit();
        EXPECT_FALSE(conn.in_transaction());
        
        // Verify data still exists after commit
        res = co_await conn.query("SELECT * FROM transaction_test");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
    });
}

// Test transaction rollback
TEST_F(PostgresqlAsyncWrapperTest, TransactionRollback) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Create a temp table outside of transaction
        result res = co_await conn.query("CREATE TEMP TABLE rollback_test (id INT, name TEXT)");
        EXPECT_TRUE(res.ok());
        
        // Begin a transaction
        co_await conn.begin_transaction();
        EXPECT_TRUE(conn.in_transaction());
        
        // Insert data
        res = co_await conn.query("INSERT INTO rollback_test VALUES (1, 'Test 1')");
        EXPECT_TRUE(res.ok());
        
        // Verify data exists within transaction
        res = co_await conn.query("SELECT * FROM rollback_test");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        
        // Roll back the transaction
        co_await conn.rollback();
        EXPECT_FALSE(conn.in_transaction());
        
        // Verify data no longer exists
        res = co_await conn.query("SELECT * FROM rollback_test");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(0, res.rows());
    });
}

// Test transaction isolation levels
TEST_F(PostgresqlAsyncWrapperTest, TransactionIsolationLevels) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Test each isolation level
        std::vector<std::pair<IsolationLevel, std::string>> levels = {
            {IsolationLevel::ReadUncommitted, "read uncommitted"},
            {IsolationLevel::ReadCommitted, "read committed"},
            {IsolationLevel::RepeatableRead, "repeatable read"},
            {IsolationLevel::Serializable, "serializable"}
        };
        
        for (const auto& [level, level_name] : levels) {
            // Begin a transaction with the specified isolation level
            co_await conn.begin_transaction(level);
            EXPECT_TRUE(conn.in_transaction());
            
            // Verify isolation level by querying the transaction_isolation setting
            result res = co_await conn.query("SHOW transaction_isolation");
            EXPECT_TRUE(res.ok());
            EXPECT_EQ(1, res.rows());
            EXPECT_STREQ(level_name.c_str(), res.get_value(0, 0));
            
            // Rollback the transaction
            co_await conn.rollback();
            EXPECT_FALSE(conn.in_transaction());
        }
    });
}

// Test nested transaction error handling
TEST_F(PostgresqlAsyncWrapperTest, NestedTransactionError) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Begin a transaction
        co_await conn.begin_transaction();
        EXPECT_TRUE(conn.in_transaction());
        
        // Attempt to begin a nested transaction
        bool caught_exception = false;
        try {
            co_await conn.begin_transaction();
        } catch (const transaction_error& e) {
            caught_exception = true;
            EXPECT_STREQ("Already in a transaction", e.what());
        }
        EXPECT_TRUE(caught_exception);
        
        // Clean up
        co_await conn.rollback();
        EXPECT_FALSE(conn.in_transaction());
    });
}

// Test commit/rollback without active transaction
TEST_F(PostgresqlAsyncWrapperTest, TransactionStateErrors) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Attempt to commit without active transaction
        bool caught_commit_exception = false;
        try {
            co_await conn.commit();
        } catch (const transaction_error& e) {
            caught_commit_exception = true;
            EXPECT_STREQ("Not in a transaction", e.what());
        }
        EXPECT_TRUE(caught_commit_exception);
        
        // Attempt to rollback without active transaction
        bool caught_rollback_exception = false;
        try {
            co_await conn.rollback();
        } catch (const transaction_error& e) {
            caught_rollback_exception = true;
            EXPECT_STREQ("Not in a transaction", e.what());
        }
        EXPECT_TRUE(caught_rollback_exception);
    });
}

// Test auto-rollback on connection close
TEST_F(PostgresqlAsyncWrapperTest, AutoRollbackOnClose) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Create temp table
        result res = co_await conn.query("CREATE TEMP TABLE auto_rollback_test (id INT)");
        EXPECT_TRUE(res.ok());
        
        // Begin transaction and add data
        co_await conn.begin_transaction();
        res = co_await conn.query("INSERT INTO auto_rollback_test VALUES (1)");
        EXPECT_TRUE(res.ok());
        
        // Verify data exists
        res = co_await conn.query("SELECT * FROM auto_rollback_test");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        
        // Close connection without committing
        conn.close();
        EXPECT_FALSE(conn.is_open());
        EXPECT_FALSE(conn.in_transaction());
        
        // Reconnect and verify data is gone
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        res = co_await conn.query("SELECT * FROM auto_rollback_test");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(0, res.rows());
    });
}

// Prepared Statement Tests

// Test basic prepared statement functionality
TEST_F(PostgresqlAsyncWrapperTest, BasicPreparedStatement) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Create a temp table
        result res = co_await conn.query("CREATE TEMP TABLE stmt_test (id INT, name TEXT)");
        EXPECT_TRUE(res.ok());
        
        // Prepare a statement
        auto stmt = co_await conn.prepare_statement("insert_stmt", "INSERT INTO stmt_test VALUES ($1, $2)");
        EXPECT_TRUE(stmt->is_prepared());
        
        // Execute the prepared statement multiple times
        co_await stmt->execute({"1", "Name 1"});
        co_await stmt->execute({"2", "Name 2"});
        co_await stmt->execute({"3", "Name 3"});
        
        // Query to verify the data
        res = co_await conn.query("SELECT * FROM stmt_test ORDER BY id");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(3, res.rows());
        
        // Verify the data
        for (int i = 0; i < 3; i++) {
            std::string expected_id = std::to_string(i + 1);
            std::string expected_name = "Name " + expected_id;
            
            EXPECT_STREQ(expected_id.c_str(), res.get_value(i, 0));
            EXPECT_STREQ(expected_name.c_str(), res.get_value(i, 1));
        }
    });
}

// Test prepare statement with execute_prepared shortcut
TEST_F(PostgresqlAsyncWrapperTest, ExecutePrepared) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Create a temp table
        result res = co_await conn.query("CREATE TEMP TABLE exec_test (id INT, name TEXT)");
        EXPECT_TRUE(res.ok());
        
        // Prepare a statement
        auto stmt = co_await conn.prepare_statement("insert_exec", "INSERT INTO exec_test VALUES ($1, $2)");
        EXPECT_TRUE(stmt->is_prepared());
        
        // Execute the prepared statement using the shortcut method
        co_await conn.execute_prepared("insert_exec", {"10", "Name 10"});
        co_await conn.execute_prepared("insert_exec", {"20", "Name 20"});
        
        // Query to verify the data
        res = co_await conn.query("SELECT * FROM exec_test ORDER BY id");
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(2, res.rows());
        
        // Verify the data
        EXPECT_STREQ("10", res.get_value(0, 0));
        EXPECT_STREQ("Name 10", res.get_value(0, 1));
        EXPECT_STREQ("20", res.get_value(1, 0));
        EXPECT_STREQ("Name 20", res.get_value(1, 1));
    });
}

// Test deallocating prepared statements
TEST_F(PostgresqlAsyncWrapperTest, DeallocateStatement) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Prepare a statement
        auto stmt = co_await conn.prepare_statement("test_dealloc", "SELECT $1::int + $2::int");
        EXPECT_TRUE(stmt->is_prepared());
        
        // Execute it
        result res = co_await stmt->execute({"10", "20"});
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_STREQ("30", res.get_value(0, 0));
        
        // Deallocate the statement
        co_await conn.deallocate_prepared("test_dealloc");
        
        // Verify the statement is no longer available
        auto stmt_ptr = conn.get_prepared_statement("test_dealloc");
        EXPECT_EQ(nullptr, stmt_ptr);
        
        // Trying to execute a deallocated statement should throw an exception
        bool caught_exception = false;
        try {
            co_await conn.execute_prepared("test_dealloc", {"5", "5"});
        } catch (const statement_error& e) {
            caught_exception = true;
            EXPECT_STREQ("Prepared statement not found: test_dealloc", e.what());
        }
        EXPECT_TRUE(caught_exception);
    });
}

// Test preparing the same statement with different SQL
TEST_F(PostgresqlAsyncWrapperTest, RepreparingStatement) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Prepare a statement
        auto stmt1 = co_await conn.prepare_statement("test_stmt", "SELECT $1::int + $2::int");
        
        // Execute it
        result res = co_await stmt1->execute({"10", "20"});
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_STREQ("30", res.get_value(0, 0));
        
        // Prepare a different statement with the same name
        auto stmt2 = co_await conn.prepare_statement("test_stmt", "SELECT $1::int * $2::int");
        
        // Execute it
        res = co_await stmt2->execute({"10", "20"});
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(1, res.rows());
        EXPECT_STREQ("200", res.get_value(0, 0));
    });
}

// Test automatic preparation of statements
TEST_F(PostgresqlAsyncWrapperTest, AutoPrepareStatement) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Create a prepared statement but don't explicitly prepare it
        auto stmt = std::make_shared<prepared_statement>(conn, "auto_prep", "SELECT $1::text || ' ' || $2::text");
        EXPECT_FALSE(stmt->is_prepared());
        
        // Execute it, should trigger automatic preparation
        result res = co_await stmt->execute({"Hello", "World"});
        EXPECT_TRUE(res.ok());
        EXPECT_TRUE(stmt->is_prepared());
        EXPECT_EQ(1, res.rows());
        EXPECT_STREQ("Hello World", res.get_value(0, 0));
    });
}

// Test deallocating all prepared statements
TEST_F(PostgresqlAsyncWrapperTest, DeallocateAllStatements) {
    connection conn(io_);
    
    run_test([&]() -> asio::awaitable<void> {
        co_await conn.connect(conn_string);
        EXPECT_TRUE(conn.is_open());
        
        // Prepare multiple statements
        co_await conn.prepare_statement("stmt1", "SELECT 1");
        co_await conn.prepare_statement("stmt2", "SELECT 2");
        co_await conn.prepare_statement("stmt3", "SELECT 3");
        
        // Verify statements exist
        EXPECT_NE(nullptr, conn.get_prepared_statement("stmt1"));
        EXPECT_NE(nullptr, conn.get_prepared_statement("stmt2"));
        EXPECT_NE(nullptr, conn.get_prepared_statement("stmt3"));
        
        // Deallocate all statements
        co_await conn.deallocate_all_prepared();
        
        // Verify all statements are gone
        EXPECT_EQ(nullptr, conn.get_prepared_statement("stmt1"));
        EXPECT_EQ(nullptr, conn.get_prepared_statement("stmt2"));
        EXPECT_EQ(nullptr, conn.get_prepared_statement("stmt3"));
    });
}
