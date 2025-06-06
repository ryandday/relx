#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/pfr.hpp>
#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/connection/postgresql_async_connection.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <relx/schema.hpp>

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

struct NameIDDTO {
  int id;
  std::string name;
};

struct IDNameEmailDTO {
  int id;
  std::string name;
  std::string email;
};

class AsyncPgIntegrationTest : public ::testing::Test {
protected:
  // Connection string for the PostgreSQL test database
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  boost::asio::io_context io_context;
  std::unique_ptr<relx::connection::PostgreSQLAsyncConnection> conn;
  Users users;

  void SetUp() override {
    // Create a new connection with the io_context
    conn = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context, conn_string);

    // Setup test environment
    run_test([this]() -> boost::asio::awaitable<void> {
      // Connect to the database
      co_await conn->connect();

      // First drop any existing table
      auto drop_sql = relx::drop_table(users).if_exists().cascade();
      co_await conn->execute(drop_sql);

      // Create the test table
      auto create_result = co_await create_test_table();
      if (create_result) {
        throw std::runtime_error("Failed to create table: " + *create_result);
      }

      // Insert test data
      auto insert_result = co_await insert_test_data();
      if (insert_result) {
        throw std::runtime_error("Failed to insert test data: " + *insert_result);
      }

      co_return;
    });
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      // Clean up test environment
      try {
        run_test([this]() -> boost::asio::awaitable<void> {
          // Make sure any active transaction is rolled back
          if (conn->in_transaction()) {
            co_await conn->rollback_transaction();
          }

          // Drop the test table
          co_await clean_test_table();

          // Disconnect
          co_await conn->disconnect();
          co_return;
        });
      } catch (const std::exception& e) {
        std::cerr << "Async cleanup failed: " << e.what() << std::endl;

        // Fallback to synchronous cleanup
        try {
          auto sync_conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
          auto connect_result = sync_conn->connect();
          if (connect_result) {
            std::string drop_sql = "DROP TABLE IF EXISTS users_async CASCADE";
            sync_conn->execute_raw(drop_sql);
            sync_conn->disconnect();
          }
        } catch (const std::exception& e) {
          std::cerr << "Synchronous fallback cleanup failed: " << e.what() << std::endl;
        }
      }
    }

    // Reset resources
    conn.reset();
    io_context.stop();
  }

  // Helper to clean up the test table asynchronously
  boost::asio::awaitable<bool> clean_test_table() {
    auto drop_sql = relx::drop_table(users).if_exists().cascade();
    co_await conn->execute(drop_sql);
    co_return true;
  }

  // Helper to create the test table asynchronously
  boost::asio::awaitable<std::optional<std::string>> create_test_table() {
    try {
      auto create_sql = relx::schema::create_table(users);
      co_await conn->execute(create_sql);
      co_return std::nullopt;
    } catch (const std::exception& e) {
      co_return std::string("Exception creating table: ") + e.what();
    }
  }

  // Helper to insert test data asynchronously
  boost::asio::awaitable<std::optional<std::string>> insert_test_data() {
    try {
      // Insert multiple rows
      auto insert_query = relx::query::insert_into(users)
                              .columns(users.name, users.email, users.age, users.active,
                                       users.score)
                              .values("John Doe", "john@example.com", 30, true, 85.5)
                              .values("Jane Smith", "jane@example.com", 28, true, 92.3)
                              .values("Bob Johnson", "bob@example.com", 35, false, 78.9)
                              .values("Alice Brown", "alice@example.com", 42, true, 91.7)
                              .values("Charlie Davis", "charlie@example.com", 25, false, 68.2);

      co_await conn->execute(insert_query);
      co_return std::nullopt;
    } catch (const std::exception& e) {
      co_return std::string("Exception inserting data: ") + e.what();
    }
  }

  // Run a test coroutine
  void run_test(std::function<boost::asio::awaitable<void>()> test_coro) {
    boost::asio::co_spawn(io_context, test_coro, [](std::exception_ptr e) {
      if (e) {
        std::rethrow_exception(e);  // Propagate exceptions
      }
    });
    io_context.run();
    io_context.restart();  // Reset io_context for next use
  }
};

TEST_F(AsyncPgIntegrationTest, SingleRowFetch) {
  run_test([this]() -> boost::asio::awaitable<void> {
    try {
      // Create a select query
      auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active,
                                       users.score)
                       .from(users)
                       .where(users.id == 1);

      // Execute the query with DTO mapping
      auto user_result = co_await conn->execute<UserDTO>(query);
      if (!user_result) {
        throw std::runtime_error("Query failed: " + user_result.error().message);
      }

      const auto& user = *user_result;

      // Check mapped struct fields
      EXPECT_EQ(1, user.id);
      EXPECT_EQ("John Doe", user.name);
      EXPECT_EQ("john@example.com", user.email);
      EXPECT_EQ(30, user.age);
      EXPECT_TRUE(user.active);
      EXPECT_DOUBLE_EQ(85.5, user.score);
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Test failure: ") + e.what());
    }
    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, MultipleRowFetch) {
  run_test([this]() -> boost::asio::awaitable<void> {
    try {
      // Create a select query for all users
      auto query = relx::query::select(users.id, users.name, users.email, users.age, users.active,
                                       users.score)
                       .from(users)
                       .order_by(users.id);

      // Execute the query with DTO mapping for multiple rows
      auto users_vec_result = co_await conn->execute_many<UserDTO>(query);
      if (!users_vec_result) {
        throw std::runtime_error("Query failed: " + users_vec_result.error().message);
      }

      const auto& users_vec = *users_vec_result;

      // Verify result
      if (users_vec.empty()) {
        throw std::runtime_error("Failed to execute query");
      }

      // Check result set
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
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Test failure: ") + e.what());
    }
    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, PartialDtoMapping) {
  run_test([this]() -> boost::asio::awaitable<void> {
    try {
      // Create a select query with only the fields needed for the partial DTO
      auto query =
          relx::query::select(users.id, users.name, users.age).from(users).where(users.id == 2);

      // Execute the query with partial DTO mapping
      auto result = co_await conn->execute<PartialUserDTO>(query);
      if (!result) {
        throw std::runtime_error("Query failed: " + result.error().message);
      }

      // Check mapped struct fields
      const auto& user = *result;
      EXPECT_EQ(2, user.id);
      EXPECT_EQ("Jane Smith", user.name);
      EXPECT_EQ(28, user.age);
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Test failure: ") + e.what());
    }
    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, ConcurrentQueries) {
  // For concurrent queries, we need separate connections
  // since libpq doesn't support concurrent queries on a single connection

  run_test([this]() -> boost::asio::awaitable<void> {
    try {
      // Create separate connections for each concurrent query
      auto conn1 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                 conn_string);
      auto conn2 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                 conn_string);
      auto conn3 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                 conn_string);

      // Connect all connections
      auto connect1 = co_await conn1->connect();
      if (!connect1)
        throw std::runtime_error("Failed to connect conn1: " + connect1.error().message);

      auto connect2 = co_await conn2->connect();
      if (!connect2)
        throw std::runtime_error("Failed to connect conn2: " + connect2.error().message);

      auto connect3 = co_await conn3->connect();
      if (!connect3)
        throw std::runtime_error("Failed to connect conn3: " + connect3.error().message);

      // Define tasks with their own connections
      auto task1 = [conn1 = conn1.get(), this]() -> boost::asio::awaitable<bool> {
        try {
          auto query = relx::query::select(users.id, users.name).from(users).where(users.id == 1);

          auto user_result = co_await conn1->execute<NameIDDTO>(query);
          if (!user_result) {
            throw std::runtime_error("Task1 query failed: " + user_result.error().message);
          }

          const auto& user = *user_result;
          EXPECT_EQ("John Doe", user.name);
          co_return true;
        } catch (const std::exception& e) {
          throw std::runtime_error(std::string("Task 1 failure: ") + e.what());
        }
      };

      auto task2 = [conn2 = conn2.get(), this]() -> boost::asio::awaitable<bool> {
        try {
          auto query = relx::query::select(users.id, users.name).from(users).where(users.id == 2);

          auto user_result = co_await conn2->execute<NameIDDTO>(query);
          if (!user_result) {
            throw std::runtime_error("Task2 query failed: " + user_result.error().message);
          }

          const auto& user = *user_result;
          EXPECT_EQ("Jane Smith", user.name);
          co_return true;
        } catch (const std::exception& e) {
          throw std::runtime_error(std::string("Task 2 failure: ") + e.what());
        }
      };

      auto task3 = [conn3 = conn3.get(), this]() -> boost::asio::awaitable<bool> {
        try {
          auto query = relx::query::select(users.id, users.name).from(users).where(users.id == 3);

          auto user_result = co_await conn3->execute<NameIDDTO>(query);
          if (!user_result) {
            throw std::runtime_error("Task3 query failed: " + user_result.error().message);
          }

          const auto& user = *user_result;
          EXPECT_EQ("Bob Johnson", user.name);
          co_return true;
        } catch (const std::exception& e) {
          throw std::runtime_error(std::string("Task 3 failure: ") + e.what());
        }
      };

      // Start all tasks concurrently and wait for them to complete
      auto result1 = co_await boost::asio::co_spawn(io_context, task1(),
                                                    boost::asio::use_awaitable);
      auto result2 = co_await boost::asio::co_spawn(io_context, task2(),
                                                    boost::asio::use_awaitable);
      auto result3 = co_await boost::asio::co_spawn(io_context, task3(),
                                                    boost::asio::use_awaitable);

      // Check results
      EXPECT_TRUE(result1);
      EXPECT_TRUE(result2);
      EXPECT_TRUE(result3);

      // Disconnect the extra connections
      auto disconnect1 = co_await conn1->disconnect();
      auto disconnect2 = co_await conn2->disconnect();
      auto disconnect3 = co_await conn3->disconnect();
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Test failure: ") + e.what());
    }
    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, TransactionSupport) {
  run_test([this]() -> boost::asio::awaitable<void> {
    try {
      // Begin a transaction
      auto begin_result = co_await conn->begin_transaction();
      if (!begin_result) {
        throw std::runtime_error("Failed to begin transaction: " + begin_result.error().message);
      }

      // Verify we're in a transaction
      EXPECT_TRUE(conn->in_transaction());

      // Insert a new record in the transaction
      auto insert_query = relx::query::insert_into(users)
                              .columns(users.name, users.email, users.age, users.active,
                                       users.score)
                              .values("Transaction Test", "transaction@example.com", 50, true,
                                      99.9);

      auto insert_result = co_await conn->execute(insert_query);
      if (!insert_result) {
        throw std::runtime_error("Insert failed: " + insert_result.error().message);
      }

      // Verify the record exists in the transaction
      auto select_query = relx::query::select(users.id, users.name, users.email)
                              .from(users)
                              .where(users.name == "Transaction Test");

      auto select_result = co_await conn->execute<IDNameEmailDTO>(select_query);
      if (!select_result) {
        throw std::runtime_error("Select failed: " + select_result.error().message);
      }

      // Rollback the transaction
      auto rollback_result = co_await conn->rollback_transaction();
      if (!rollback_result) {
        throw std::runtime_error("Rollback failed: " + rollback_result.error().message);
      }

      // Verify we're no longer in a transaction
      EXPECT_FALSE(conn->in_transaction());

      // Verify the record doesn't exist after rollback
      auto verify_query = relx::query::select(users.id, users.name, users.email)
                              .from(users)
                              .where(users.name == "Transaction Test");

      auto verify_result = co_await conn->execute_many<IDNameEmailDTO>(verify_query);
      if (!verify_result) {
        throw std::runtime_error("Verify query failed: " + verify_result.error().message);
      }

      // Should be empty after rollback
      EXPECT_TRUE((*verify_result).empty());
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Test failure: ") + e.what());
    }
    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, ConnectionParamsConstructor) {
  run_test([this]() -> boost::asio::awaitable<void> {
    try {
      // Create connection parameters
      relx::connection::PostgreSQLConnectionParams params;
      params.host = "localhost";
      params.port = 5434;
      params.dbname = "relx_test";
      params.user = "postgres";
      params.password = "postgres";
      params.application_name = "async_params_test";

      // Create a connection with parameters
      auto param_conn = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                      params);

      // Connect and verify it works
      auto connect_result = co_await param_conn->connect();
      if (!connect_result) {
        throw std::runtime_error("Connection failed: " + connect_result.error().message);
      }

      // Execute a simple query to verify connection
      auto result = co_await param_conn->execute_raw("SELECT 1 as value");
      if (!result) {
        throw std::runtime_error("Query failed: " + result.error().message);
      }

      EXPECT_EQ(1, result->size());

      auto cell_result = result->at(0).get_cell(0);
      EXPECT_TRUE(cell_result);
      EXPECT_EQ("1", (*cell_result)->raw_value());

      // Disconnect
      co_await param_conn->disconnect();
      EXPECT_FALSE(param_conn->is_connected());

    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Test failure: ") + e.what());
    }
    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, TestAsyncConnectionErrorHandling) {
  run_test([this]() -> boost::asio::awaitable<void> {
    // Test 1: Connection with invalid parameters
    {
      auto bad_conn = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(
          io_context, "invalid connection string");

      auto connect_result = co_await bad_conn->connect();
      EXPECT_FALSE(connect_result);
      EXPECT_FALSE(bad_conn->is_connected());

      // Try to execute on bad connection
      auto exec_result = co_await bad_conn->execute_raw("SELECT 1");
      EXPECT_FALSE(exec_result);
    }

    // Test 2: Double connect (should be safe)
    {
      auto connect_result1 = co_await conn->connect();
      EXPECT_TRUE(connect_result1);

      auto connect_result2 = co_await conn->connect();  // Should be safe
      EXPECT_TRUE(connect_result2);
      EXPECT_TRUE(conn->is_connected());
    }

    // Test 3: Execute on disconnected connection
    {
      co_await conn->disconnect();
      EXPECT_FALSE(conn->is_connected());

      auto exec_result = co_await conn->execute_raw("SELECT 1");
      EXPECT_FALSE(exec_result);
      EXPECT_NE("", exec_result.error().message);

      // Reconnect for further tests
      co_await conn->connect();
    }

    // Test 4: Transaction error handling
    {
      // Test begin transaction on disconnected connection
      co_await conn->disconnect();
      auto begin_result = co_await conn->begin_transaction();
      EXPECT_FALSE(begin_result);

      // Reconnect
      co_await conn->connect();

      // Test commit without active transaction
      auto commit_result = co_await conn->commit_transaction();
      EXPECT_FALSE(commit_result);

      // Test rollback without active transaction
      auto rollback_result = co_await conn->rollback_transaction();
      EXPECT_FALSE(rollback_result);

      // Test nested transaction (should fail)
      auto begin_result1 = co_await conn->begin_transaction();
      EXPECT_TRUE(begin_result1);
      EXPECT_TRUE(conn->in_transaction());

      auto begin_result2 = co_await conn->begin_transaction();  // Should fail
      EXPECT_FALSE(begin_result2);

      // Clean up transaction
      auto rollback_result2 = co_await conn->rollback_transaction();
      EXPECT_TRUE(rollback_result2);
      EXPECT_FALSE(conn->in_transaction());
    }

    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, TestAsyncConnectionMoveSemantics) {
  run_test([this]() -> boost::asio::awaitable<void> {
    // Test move constructor
    {
      auto conn1 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                 conn_string);

      auto connect_result = co_await conn1->connect();
      EXPECT_TRUE(connect_result);
      EXPECT_TRUE(conn1->is_connected());

      // Move construct
      auto conn2 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(std::move(*conn1));

      EXPECT_FALSE(conn1->is_connected());  // Original should be disconnected
      EXPECT_TRUE(conn2->is_connected());   // Moved-to should be connected

      // Test that moved-to connection works
      auto exec_result = co_await conn2->execute_raw("SELECT 1 as value");
      EXPECT_TRUE(exec_result);

      co_await conn2->disconnect();
    }

    // Test move assignment
    {
      auto conn1 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                 conn_string);
      auto conn2 = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                 "dummy");

      auto connect_result = co_await conn1->connect();
      EXPECT_TRUE(connect_result);
      EXPECT_TRUE(conn1->is_connected());

      // Move assign
      *conn2 = std::move(*conn1);

      EXPECT_FALSE(conn1->is_connected());  // Original should be disconnected
      EXPECT_TRUE(conn2->is_connected());   // Moved-to should be connected

      // Test that moved-to connection works
      auto exec_result = co_await conn2->execute_raw("SELECT 1 as value");
      EXPECT_TRUE(exec_result);

      co_await conn2->disconnect();
    }

    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, TestAsyncTransactionIsolationLevels) {
  run_test([this]() -> boost::asio::awaitable<void> {
    co_await conn->connect();

    // Test all isolation levels
    std::vector<relx::connection::IsolationLevel> levels = {
        relx::connection::IsolationLevel::ReadUncommitted,
        relx::connection::IsolationLevel::ReadCommitted,
        relx::connection::IsolationLevel::RepeatableRead,
        relx::connection::IsolationLevel::Serializable};

    for (auto level : levels) {
      auto begin_result = co_await conn->begin_transaction(level);
      EXPECT_TRUE(begin_result);
      EXPECT_TRUE(conn->in_transaction());

      // Execute a simple query within the transaction
      auto exec_result = co_await conn->execute_raw("SELECT 1 as test_value");
      EXPECT_TRUE(exec_result);

      auto rollback_result = co_await conn->rollback_transaction();
      EXPECT_TRUE(rollback_result);
      EXPECT_FALSE(conn->in_transaction());
    }

    co_return;
  });
}

TEST_F(AsyncPgIntegrationTest, TestAsyncConnectionDestructorCleanup) {
  run_test([this]() -> boost::asio::awaitable<void> {
    // Test that destructor properly handles connected state
    {
      auto temp_conn = std::make_unique<relx::connection::PostgreSQLAsyncConnection>(io_context,
                                                                                     conn_string);

      auto connect_result = co_await temp_conn->connect();
      EXPECT_TRUE(connect_result);

      // Start a transaction
      auto begin_result = co_await temp_conn->begin_transaction();
      EXPECT_TRUE(begin_result);

      // Let the connection go out of scope while in transaction
      // Destructor should handle cleanup gracefully
    }  // temp_conn destructor called here

    // Test that we can still use other connections normally
    auto exec_result = co_await conn->execute_raw("SELECT 1 as cleanup_test");
    EXPECT_TRUE(exec_result);

    co_return;
  });
}

}  // namespace