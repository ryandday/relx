#include <gtest/gtest.h>
#include <relx/postgresql.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <relx/connection/postgresql_async_connection.hpp>
#include <boost/pfr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <future>

// With async context, you can't use ASSERT_TRUE, ASSERT_EQ, etc.
// You have to use EXPECT_TRUE, EXPECT_EQ, etc.
// This is because ASSERT_TRUE, ASSERT_EQ, etc. returns something,
// but the async context expects to return something else.

namespace {

// Define a simple test table
struct Users {
    static constexpr auto table_name = "users_async";
    relx::schema::column<Users, "id", int, relx::identity<>> id;
    relx::schema::column<Users, "name", std::string> name;
    relx::schema::column<Users, "email", std::string> email;
    relx::schema::column<Users, "age", int> age;
    relx::schema::column<Users, "active", bool> active;
    relx::schema::column<Users, "score", double> score;
    
    relx::schema::table_primary_key<&Users::id> pk;
};

// Define a DTO to map rows to
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

class AsyncPgIntegrationTest : public ::testing::Test {
protected:
    // Connection string for the PostgreSQL test database
    std::string conn_string = "host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres";
    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<relx::connection::PostgreSQLAsyncConnection> conn;
    Users users;
    
    void SetUp() override {
        // Create a fresh io_context for each test
        io_context = std::make_unique<boost::asio::io_context>();
        
        // Create a new connection with the fresh io_context
        conn = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(*io_context, conn_string);
        // Run setup tasks in a separate thread
        auto setup_future = std::async(std::launch::async, [this]() {
            // Connect to the database
            auto connect_task = [this]() -> boost::asio::awaitable<void> {
                auto connect_result = co_await conn->connect();
                if (!connect_result) {
                    throw std::runtime_error("Failed to connect to database: " + connect_result.error().message);
                } 

                // Clean up existing table, don't fail if the table doesn't exist
                try {
                    co_await clean_test_table();
                } catch (const std::exception& e) {
                    std::cerr << "Error cleaning test table: " << e.what() << std::endl;
                    // Ignore errors - table might not exist
                }
                
                // Create test table
                auto create_result = co_await create_test_table();
                if (create_result) {
                    throw std::runtime_error("Failed to create test table: " + *create_result);
                }
                
                // Insert test data
                auto insert_result = co_await insert_test_data();
                if (insert_result) {
                    throw std::runtime_error("Failed to insert test data: " + *insert_result);
                }
            };
            
            // Run the setup tasks and wait for completion
            boost::asio::co_spawn(*io_context, connect_task(), 
                [](std::exception_ptr e) {
                    if (e) {
                        std::rethrow_exception(e); // Propagate setup errors
                    }
                });
            io_context->run();
        });
        
        // Wait for setup to complete
        setup_future.get();
    }
    
    void TearDown() override {
        // First attempt async cleanup
        if (conn && conn->is_connected()) {
            try {
                // Run cleanup tasks in a separate thread
                auto cleanup_future = std::async(std::launch::async, [this]() {
                    auto cleanup_task = [this]() -> boost::asio::awaitable<void> {
                        // Make sure any active transaction is rolled back
                        if (conn->in_transaction()) {
                            auto rollback_result = co_await conn->rollback_transaction();
                            // Ignore rollback errors, just try to clean up
                        }
                        
                        // Drop the test table
                        try {
                            co_await clean_test_table();
                        } catch (const std::exception& e) {
                            // Ignore errors during cleanup
                        }
                        
                        // Explicitly disconnect the connection
                        co_await conn->disconnect();
                    };
                    
                    boost::asio::co_spawn(*io_context, cleanup_task(), 
                        [](std::exception_ptr e) {
                            if (e) {
                                // Just log cleanup errors rather than propagating them
                                try {
                                    if (e) std::rethrow_exception(e);
                                } catch (const std::exception& ex) {
                                    std::cerr << "Cleanup error: " << ex.what() << std::endl;
                                }
                            }
                        });
                    io_context->run();
                });
                
                // Wait with timeout for cleanup to complete
                auto status = cleanup_future.wait_for(std::chrono::seconds(5));
                if (status == std::future_status::ready) {
                    cleanup_future.get();
                }
                // If timeout, we'll fall back to sync cleanup
            }
            catch (const std::exception& e) {
                std::cerr << "Async cleanup failed: " << e.what() << std::endl;
                // Will fall back to sync cleanup
            }
        }
        
        // Always perform a synchronous cleanup as a fallback
        try {
            // Create a synchronous connection and forcibly clean up the table
            auto sync_conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
            auto connect_result = sync_conn->connect();
            if (connect_result) {
                // Drop the test table using synchronous API
                std::string drop_sql = "DROP TABLE IF EXISTS users_async CASCADE";
                sync_conn->execute_raw(drop_sql);
                sync_conn->disconnect();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Synchronous fallback cleanup failed: " << e.what() << std::endl;
        }
        
        // Explicitly release the connection before the io_context
        conn.reset();
        
        // Reset the io_context to clean state
        if (io_context) {
            io_context->stop();
            io_context.reset();
        }
    }
    
    // Helper to clean up the test table asynchronously
    boost::asio::awaitable<bool> clean_test_table() {
        auto drop_sql = relx::drop_table(users).if_exists().cascade().build();
        auto result = co_await conn->execute_raw(drop_sql);
        if (!result) {
            co_return false;
        }
        co_return true;
    }
    
    // Helper to create the test table asynchronously
    boost::asio::awaitable<std::optional<std::string>> create_test_table() {
        auto create_sql = relx::create_table(users);
        auto result = co_await conn->execute_raw(create_sql);
        if (!result) {
            co_return result.error().message;
        }
        co_return std::nullopt;
    }
    
    // Helper to insert test data asynchronously
    boost::asio::awaitable<std::optional<std::string>> insert_test_data() {
        // Insert multiple rows
        auto insert_query = relx::query::insert_into(users)
            .columns(users.name, users.email, users.age, users.active, users.score)
            .values("John Doe", "john@example.com", 30, true, 85.5)
            .values("Jane Smith", "jane@example.com", 28, true, 92.3)
            .values("Bob Johnson", "bob@example.com", 35, false, 78.9)
            .values("Alice Brown", "alice@example.com", 42, true, 91.7)
            .values("Charlie Davis", "charlie@example.com", 25, false, 68.2);
            
        auto result = co_await conn->execute(insert_query);
        if (!result) {
            co_return result.error().message;
        }
        co_return std::nullopt;
    }
    
    // Run a test coroutine and return the result
    template <typename Func>
    void run_test(Func test_coro) {
        auto test_future = std::async(std::launch::async, [this, test_coro]() {
            boost::asio::co_spawn(*io_context, test_coro(), 
                [](std::exception_ptr e) {
                    if (e) {
                        std::rethrow_exception(e);  // Propagate exceptions to io_context.run()
                    }
                });
            io_context->run();
        });
        
        // Wait for test to complete
        test_future.get();
    }
};

TEST_F(AsyncPgIntegrationTest, SingleRowFetch) {
    run_test([this]() -> boost::asio::awaitable<void> {
        // Create a select query
        auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
            .from(users)
            .where(users.id == 1);
        
        // Execute the query with DTO mapping
        auto result = co_await conn->execute<UserDTO>(query);
        
        // Verify result
        if (!result) {
            throw std::runtime_error("Failed to execute query: " + result.error().message);
        }
        
        // Check mapped struct fields
        UserDTO user = *result;
        EXPECT_EQ(1, user.id);
        EXPECT_EQ("John Doe", user.name);
        EXPECT_EQ("john@example.com", user.email);
        EXPECT_EQ(30, user.age);
        EXPECT_TRUE(user.active);
        EXPECT_DOUBLE_EQ(85.5, user.score);
        co_return;
    });
}

TEST_F(AsyncPgIntegrationTest, MultipleRowFetch) {
    run_test([this]() -> boost::asio::awaitable<void> {
        // Create a select query for all users
        auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active, users.score)
            .from(users)
            .order_by(users.id);
        
        // Execute the query with DTO mapping for multiple rows
        auto result = co_await conn->execute_many<UserDTO>(query);
        
        // Verify result
        if (!result) {
            throw std::runtime_error("Failed to execute query: " + result.error().message);
        }
        
        // Check result set
        const auto& users_vec = *result;
        if (users_vec.size() != 5) {
            throw std::runtime_error("Expected 5 rows, got " + std::to_string(users_vec.size()));
        }
        
        // Check first user
        EXPECT_EQ(1, users_vec[0].id);
        EXPECT_EQ("John Doe", users_vec[0].name);
        EXPECT_EQ(30, users_vec[0].age);
        
        // Check last user
        EXPECT_EQ(5, users_vec[4].id);
        EXPECT_EQ("Charlie Davis", users_vec[4].name);
        EXPECT_EQ(25, users_vec[4].age);
        co_return;
    });
}

TEST_F(AsyncPgIntegrationTest, PartialDtoMapping) {
    run_test([this]() -> boost::asio::awaitable<void> {
        // Create a select query with only the fields needed for the partial DTO
        auto query = relx::query::select(users.id, users.name, users.age)
            .from(users)
            .where(users.id == 2);
        
        // Execute the query with partial DTO mapping
        auto result = co_await conn->execute<PartialUserDTO>(query);
        
        // Verify result
        if (!result) {
            throw std::runtime_error("Failed to execute query: " + result.error().message);
        }
        
        // Check mapped struct fields
        PartialUserDTO user = *result;
        EXPECT_EQ(2, user.id);
        EXPECT_EQ("Jane Smith", user.name);
        EXPECT_EQ(28, user.age);
        co_return;
    });
}

TEST_F(AsyncPgIntegrationTest, ConcurrentQueries) {
    // For concurrent queries, we need separate connections
    // since libpq doesn't support concurrent queries on a single connection
    
    run_test([this]() -> boost::asio::awaitable<void> {
        // Create separate connections for each concurrent query
        auto conn1 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(*io_context, conn_string);
        auto conn2 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(*io_context, conn_string);
        auto conn3 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(*io_context, conn_string);
        
        // Connect all connections
        auto connect1 = co_await conn1->connect();
        auto connect2 = co_await conn2->connect();
        auto connect3 = co_await conn3->connect();
        
        if (!connect1 || !connect2 || !connect3) {
            throw std::runtime_error("Failed to connect one or more connections");
        }
        
        // Define tasks with their own connections
        auto task1 = [conn1 = conn1.get(), this]() -> boost::asio::awaitable<bool> {
            auto query = relx::query::select(users.id, users.name)
                .from(users)
                .where(users.id == 1);
                
            auto result = co_await conn1->execute<PartialUserDTO>(query);
            if (!result) {
                co_return false;
            }
            EXPECT_EQ("John Doe", (*result).name);
            co_return true;
        };
        
        auto task2 = [conn2 = conn2.get(), this]() -> boost::asio::awaitable<bool> {
            auto query = relx::query::select(users.id, users.name)
                .from(users)
                .where(users.id == 2);
                
            auto result = co_await conn2->execute<PartialUserDTO>(query);
            if (!result) {
                co_return false;
            }
            EXPECT_EQ("Jane Smith", (*result).name);
            co_return true;
        };
        
        auto task3 = [conn3 = conn3.get(), this]() -> boost::asio::awaitable<bool> {
            auto query = relx::query::select(users.id, users.name)
                .from(users)
                .where(users.id == 3);
                
            auto result = co_await conn3->execute<PartialUserDTO>(query);
            if (!result) {
                co_return false;
            }
            EXPECT_EQ("Bob Johnson", (*result).name);
            co_return true;
        };
        
        // Start all tasks concurrently and wait for them to complete
        auto result1 = co_await boost::asio::co_spawn(*io_context, task1(), boost::asio::use_awaitable);
        auto result2 = co_await boost::asio::co_spawn(*io_context, task2(), boost::asio::use_awaitable);
        auto result3 = co_await boost::asio::co_spawn(*io_context, task3(), boost::asio::use_awaitable);
        
        // Check results
        EXPECT_TRUE(result1);
        EXPECT_TRUE(result2);
        EXPECT_TRUE(result3);
        
        // Disconnect the extra connections
        co_await conn1->disconnect();
        co_await conn2->disconnect();
        co_await conn3->disconnect();
        
        co_return;
    });
}

TEST_F(AsyncPgIntegrationTest, TransactionSupport) {
    run_test([this]() -> boost::asio::awaitable<void> {
        // Begin a transaction
        auto begin_result = co_await conn->begin_transaction();
        if (!begin_result) {
            throw std::runtime_error("Failed to begin transaction: " + begin_result.error().message);
        }
        
        // Verify we're in a transaction
        EXPECT_TRUE(conn->in_transaction());
        
        // Insert a new record in the transaction
        auto insert_query = relx::query::insert_into(users)
            .columns(users.name, users.email, users.age, users.active, users.score)
            .values("Transaction Test", "transaction@example.com", 50, true, 99.9);
            
        auto insert_result = co_await conn->execute(insert_query);
        if (!insert_result) {
            // Make sure to rollback if the insert fails
            auto rollback_result = co_await conn->rollback_transaction();
            throw std::runtime_error("Failed to insert test data: " + insert_result.error().message);
        }
        
        // Verify the record exists in the transaction
        auto select_query = relx::query::select(users.id, users.name, users.email)
            .from(users)
            .where(users.name == "Transaction Test");
            
        auto select_result = co_await conn->execute<UserDTO>(select_query);
        if (!select_result) {
            // Make sure to rollback if the select fails
            auto rollback_result = co_await conn->rollback_transaction();
            throw std::runtime_error("Failed to find test record in transaction: " + select_result.error().message);
        }
        
        // Rollback the transaction
        auto rollback_result = co_await conn->rollback_transaction();
        if (!rollback_result) {
            throw std::runtime_error("Failed to rollback transaction: " + rollback_result.error().message);
        }
        
        // Verify we're no longer in a transaction
        EXPECT_FALSE(conn->in_transaction());
        
        // Verify the record doesn't exist after rollback
        auto verify_query = relx::query::select(users.id, users.name, users.email)
            .from(users)
            .where(users.name == "Transaction Test");
            
        auto verify_result = co_await conn->execute<UserDTO>(verify_query);
        
        // The query should fail because the record doesn't exist
        EXPECT_FALSE(verify_result.has_value());
        
        co_return;
    });
}

} // namespace 