#include "schema_definitions.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/connection/postgresql_connection_pool.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Test fixture for PostgreSQL connection pool integration tests
class ConnectionPoolIntegrationTest : public ::testing::Test {
protected:
  using ConnectionPool = relx::connection::PostgreSQLConnectionPool;
  using Connection = relx::connection::PostgreSQLConnection;
  using PoolConfig = relx::connection::PostgreSQLConnectionPoolConfig;

  std::unique_ptr<ConnectionPool> pool;

  void SetUp() override {
    // Configure the connection pool
    PoolConfig config;
    config.connection_string =
        "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
    config.initial_size = 3;
    config.max_size = 10;
    config.connection_timeout = std::chrono::milliseconds(5000);
    config.validate_connections = true;

    // Create and initialize the pool
    pool = std::make_unique<ConnectionPool>(config);
    auto result = pool->initialize();
    ASSERT_TRUE(result) << "Failed to initialize connection pool: " << result.error().message;

    // Clean up any existing tables by using a connection from the pool
    cleanup_database();
  }

  void TearDown() override {
    // Pool will be automatically cleaned up
  }

  void cleanup_database() {
    auto result = pool->with_connection([](auto conn) {
      auto result = conn->execute_raw("DROP TABLE IF EXISTS orders CASCADE");
      if (!result) return result.error();

      result = conn->execute_raw("DROP TABLE IF EXISTS inventory CASCADE");
      if (!result) return result.error();

      result = conn->execute_raw("DROP TABLE IF EXISTS customers CASCADE");
      if (!result) return result.error();

      result = conn->execute_raw("DROP TABLE IF EXISTS products CASCADE");
      if (!result) return result.error();

      result = conn->execute_raw("DROP TABLE IF EXISTS categories CASCADE");
      if (!result) return result.error();

      // Return success
      return relx::connection::ConnectionError{};
    });

    ASSERT_TRUE(result) << "Failed to cleanup database: " << result.error().message;
  }

  void setup_test_schema() {
    // Create a simple test table for connection pool tests
    auto result = pool->with_connection([](auto conn) {
      auto result = conn->execute_raw("CREATE TABLE IF NOT EXISTS test_pool ("
                                      "  id SERIAL PRIMARY KEY,"
                                      "  value TEXT NOT NULL,"
                                      "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                                      ")");

      if (!result) return result.error();

      // Return success
      return relx::connection::ConnectionError{};
    });

    ASSERT_TRUE(result) << "Failed to create test table: " << result.error().message;
  }
};

// Basic connection pool functionality test
TEST_F(ConnectionPoolIntegrationTest, BasicPoolFunctionality) {
  // Set up a test table
  setup_test_schema();

  // Get a connection from the pool
  auto conn_result = pool->get_connection();
  ASSERT_TRUE(conn_result) << "Failed to get connection from pool: " << conn_result.error().message;

  auto conn = *conn_result;
  ASSERT_TRUE(conn->is_connected()) << "Connection should be connected";

  // Execute a query with the connection
  auto result = conn->execute_raw("INSERT INTO test_pool (value) VALUES ('test1') RETURNING id");
  ASSERT_TRUE(result) << "Failed to insert test data: " << result.error().message;

  auto& rows = *result;
  ASSERT_EQ(1, rows.size());
  auto id = rows[0].get<int>(0);
  ASSERT_TRUE(id.has_value());

  // Return the connection to the pool
  pool->return_connection(std::move(conn));

  // Check active and idle connections
  EXPECT_EQ(0, pool->active_connections()) << "Should have 0 active connections after return";
  EXPECT_GE(pool->idle_connections(), 1) << "Should have at least 1 idle connection after return";

  // Get another connection and verify data
  auto conn_result2 = pool->get_connection();
  ASSERT_TRUE(conn_result2) << "Failed to get second connection from pool: "
                            << conn_result2.error().message;

  auto conn2 = *conn_result2;

  // Verify data with the new connection
  result = conn2->execute_raw("SELECT value FROM test_pool WHERE id = ?", {std::to_string(*id)});
  ASSERT_TRUE(result) << "Failed to select test data: " << result.error().message;

  auto& data_rows = *result;
  ASSERT_EQ(1, data_rows.size());
  auto value = data_rows[0].get<std::string>(0);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ("test1", *value);

  // Return the second connection
  pool->return_connection(std::move(conn2));
}

// Test with_connection helper
TEST_F(ConnectionPoolIntegrationTest, WithConnectionHelper) {
  // Set up a test table
  setup_test_schema();

  // Use the with_connection helper
  auto result = pool->with_connection([](auto conn) -> relx::ConnectionPoolResult<int> {
    // Insert a record
    auto insert_result = conn->execute_raw(
        "INSERT INTO test_pool (value) VALUES ('with_connection_test') RETURNING id");

    if (!insert_result) {
      return insert_result.error();
    }

    auto& rows = *insert_result;
    int id = *(rows[0].template get<int>(0));

    // Return the ID
    return id;
  });

  ASSERT_TRUE(result) << "Failed to execute with_connection: " << result.error().message;
  ASSERT_TRUE(*result) << "Failed to execute with_connection: " << result.error().message;
  int inserted_id = **result;

  // Verify data with another with_connection call
  auto verify_result = pool->with_connection([inserted_id](auto conn) {
    auto select_result = conn->execute_raw("SELECT value FROM test_pool WHERE id = ?",
                                           {std::to_string(inserted_id)});

    if (!select_result) {
      return std::make_pair(false, select_result.error().message);
    }

    auto& rows = *select_result;
    if (rows.size() != 1) {
      return std::make_pair(false,
                            std::string("Expected 1 row, got ") + std::to_string(rows.size()));
    }

    auto value = rows[0].get<std::string>(0);
    if (!value) {
      return std::make_pair(false, "NULL value");
    }

    return std::make_pair(true, *value);
  });

  ASSERT_TRUE(verify_result) << "Failed to verify data: " << verify_result.error().message;

  auto [success, value] = *verify_result;
  EXPECT_TRUE(success);
  EXPECT_EQ("with_connection_test", value);
}

// Test concurrent access to connection pool
TEST_F(ConnectionPoolIntegrationTest, ConcurrentAccess) {
  // Set up a test table
  setup_test_schema();

  const int thread_count = 10;
  const int iterations_per_thread = 5;

  std::vector<std::thread> threads;
  std::mutex result_mutex;
  std::vector<int> inserted_ids;
  std::atomic<int> success_count(0);
  std::atomic<int> error_count(0);

  // Launch multiple threads that use the connection pool
  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([this, t, &result_mutex, &inserted_ids, &success_count, &error_count,
                          iterations_per_thread]() {
      for (int i = 0; i < iterations_per_thread; ++i) {
        // Use with_connection to perform a transaction
        auto result = pool->with_connection([t, i](auto conn) {
          // Start a transaction
          auto tx_result = conn->begin_transaction();
          if (!tx_result) {
            return std::make_pair(false, tx_result.error().message);
          }

          // Insert a record
          auto value = "thread_" + std::to_string(t) + "_iter_" + std::to_string(i);
          auto insert_result = conn->execute_raw(
              "INSERT INTO test_pool (value) VALUES (?) RETURNING id", {value});

          if (!insert_result) {
            conn->rollback_transaction();
            return std::make_pair(false, insert_result.error().message);
          }

          auto& rows = *insert_result;
          int id = *(rows[0].get<int>(0));

          // Commit the transaction
          auto commit_result = conn->commit_transaction();
          if (!commit_result) {
            return std::make_pair(false, commit_result.error().message);
          }

          return std::make_pair(true, id);
        });

        if (result) {
          auto [success, id_or_error] = *result;
          if (success) {
            {
              std::lock_guard<std::mutex> lock(result_mutex);
              inserted_ids.push_back(id_or_error);
            }
            success_count++;
          } else {
            error_count++;
          }
        } else {
          error_count++;
        }

        // Add small random delay between operations
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 20));
      }
    });
  }

  // Wait for all threads to complete
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all operations succeeded
  EXPECT_EQ(thread_count * iterations_per_thread, success_count)
      << "Expected all operations to succeed, but got " << error_count << " errors";

  // Verify we have the expected number of records
  auto count_result = pool->with_connection([](auto conn) {
    auto result = conn->execute_raw("SELECT COUNT(*) FROM test_pool");
    if (!result) {
      return -1;
    }

    auto& rows = *result;
    return *(rows[0].get<int>(0));
  });

  ASSERT_TRUE(count_result) << "Failed to count records: " << count_result.error().message;
  EXPECT_EQ(thread_count * iterations_per_thread, *count_result)
      << "Expected " << (thread_count * iterations_per_thread) << " records";

  // Verify all inserted IDs are unique
  std::sort(inserted_ids.begin(), inserted_ids.end());
  auto it = std::adjacent_find(inserted_ids.begin(), inserted_ids.end());
  EXPECT_EQ(inserted_ids.end(), it) << "Found duplicate ID: " << *it;
}

// Test connection pool under load
TEST_F(ConnectionPoolIntegrationTest, PoolUnderLoad) {
  // Set up a test table
  setup_test_schema();

  // Create more connections than the initial pool size to test growth
  const int connection_count = 8;  // Greater than initial_size but less than max_size

  std::vector<std::shared_ptr<Connection>> connections;

  // Get multiple connections from the pool
  for (int i = 0; i < connection_count; ++i) {
    auto result = pool->get_connection();
    ASSERT_TRUE(result) << "Failed to get connection " << i << ": " << result.error().message;
    connections.push_back(*result);

    // Verify connection works
    auto query_result = connections.back()->execute_raw("SELECT 1");
    ASSERT_TRUE(query_result) << "Connection " << i << " failed basic query";
  }

  // Check pool stats
  EXPECT_EQ(connection_count, pool->active_connections())
      << "Expected " << connection_count << " active connections";

  // Return some connections
  for (int i = 0; i < connection_count / 2; ++i) {
    pool->return_connection(std::move(connections[i]));
  }

  // Check pool stats after returns
  EXPECT_EQ(connection_count - (connection_count / 2), pool->active_connections())
      << "Expected " << (connection_count - (connection_count / 2))
      << " active connections after returns";

  // Return remaining connections
  for (int i = connection_count / 2; i < connection_count; ++i) {
    if (connections[i]) {
      pool->return_connection(std::move(connections[i]));
    }
  }

  // Check all connections returned
  EXPECT_EQ(0, pool->active_connections()) << "Expected 0 active connections after all returns";
  EXPECT_GE(pool->idle_connections(), 3) << "Expected at least initial_size idle connections";
}

// Test connection validation
TEST_F(ConnectionPoolIntegrationTest, ConnectionValidation) {
  // Set up a test table
  setup_test_schema();

  // Get a connection and simulate failure
  auto conn_result = pool->get_connection();
  ASSERT_TRUE(conn_result) << "Failed to get connection: " << conn_result.error().message;

  auto conn = *conn_result;

  // Force disconnect from the server
  conn->disconnect();

  // Return the broken connection to the pool
  pool->return_connection(std::move(conn));

  // Try to get a new connection
  // The pool should validate and create a new one since the returned one was broken
  auto new_conn_result = pool->get_connection();
  ASSERT_TRUE(new_conn_result) << "Failed to get new connection after broken one: "
                               << new_conn_result.error().message;

  auto new_conn = *new_conn_result;

  // Verify the new connection works
  auto query_result = new_conn->execute_raw("SELECT 1 as test");
  ASSERT_TRUE(query_result) << "New connection failed basic query: "
                            << query_result.error().message;

  auto& rows = *query_result;
  ASSERT_EQ(1, rows.size());
  EXPECT_EQ(1, *rows[0].get<int>(0));

  // Return the working connection
  pool->return_connection(std::move(new_conn));
}

// Test transaction management within the pool
TEST_F(ConnectionPoolIntegrationTest, PoolTransactions) {
  // Set up a test table
  setup_test_schema();

  // Use with_connection to perform a transaction
  auto result = pool->with_connection([](auto conn) {
    // Start a transaction
    auto tx_result = conn->begin_transaction();
    if (!tx_result) {
      return false;
    }

    // Insert two records
    auto insert1 = conn->execute_raw("INSERT INTO test_pool (value) VALUES ('tx_test_1')");
    if (!insert1) {
      conn->rollback_transaction();
      return false;
    }

    auto insert2 = conn->execute_raw("INSERT INTO test_pool (value) VALUES ('tx_test_2')");
    if (!insert2) {
      conn->rollback_transaction();
      return false;
    }

    // Commit the transaction
    auto commit_result = conn->commit_transaction();
    return commit_result.has_value();
  });

  ASSERT_TRUE(result) << "Failed to execute transaction: " << result.error().message;
  EXPECT_TRUE(*result) << "Transaction should have committed successfully";

  // Verify both records exist
  auto verify_result = pool->with_connection([](auto conn) {
    auto select_result = conn->execute_raw(
        "SELECT value FROM test_pool WHERE value LIKE 'tx_test_%' ORDER BY value");

    if (!select_result) {
      return std::vector<std::string>();
    }

    auto& rows = *select_result;
    std::vector<std::string> values;

    for (const auto& row : rows) {
      auto value = row.get<std::string>(0);
      if (value) {
        values.push_back(*value);
      }
    }

    return values;
  });

  ASSERT_TRUE(verify_result) << "Failed to verify transaction data: "
                             << verify_result.error().message;

  auto values = *verify_result;
  ASSERT_EQ(2, values.size()) << "Expected 2 values from transaction";
  EXPECT_EQ("tx_test_1", values[0]);
  EXPECT_EQ("tx_test_2", values[1]);

  // Test rolling back a transaction
  auto rollback_result = pool->with_connection([](auto conn) {
    // Start a transaction
    auto tx_result = conn->begin_transaction();
    if (!tx_result) {
      return false;
    }

    // Insert a record
    auto insert = conn->execute_raw(
        "INSERT INTO test_pool (value) VALUES ('should_be_rolled_back')");
    if (!insert) {
      conn->rollback_transaction();
      return false;
    }

    // Rollback the transaction
    auto rollback_result = conn->rollback_transaction();
    return rollback_result.has_value();
  });

  ASSERT_TRUE(rollback_result) << "Failed to execute rollback test: "
                               << rollback_result.error().message;
  EXPECT_TRUE(*rollback_result) << "Rollback should have succeeded";

  // Verify rollback worked - the record should not exist
  auto rollback_verify = pool->with_connection([](auto conn) {
    auto select_result = conn->execute_raw(
        "SELECT COUNT(*) FROM test_pool WHERE value = 'should_be_rolled_back'");

    if (!select_result) {
      return -1;
    }

    auto& rows = *select_result;
    return *(rows[0].get<int>(0));
  });

  ASSERT_TRUE(rollback_verify) << "Failed to verify rollback: " << rollback_verify.error().message;
  EXPECT_EQ(0, *rollback_verify) << "Rolled back record should not exist";
}