#include "relx/connection/postgresql_connection.hpp"
#include "relx/connection/postgresql_streaming_source.hpp"
#include "relx/query.hpp"
#include "relx/results/lazy_result.hpp"
#include "relx/results/streaming_result.hpp"
#include "relx/schema.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace relx::test {

// Test table schema for users
struct Users {
  schema::column<Users, "id", int> id;
  schema::column<Users, "name", std::string> name;
  schema::column<Users, "email", std::string> email;
  schema::column<Users, "age", int> age;

  static constexpr std::string_view table_name = "users";
};

// Test fixture for PostgreSQL streaming tests
class PostgreSQLStreamingTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Use a test database connection string
    // Note: These tests require a running PostgreSQL instance
    std::string conn_str =
        "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";

    try {
      connection = std::make_unique<connection::PostgreSQLConnection>(conn_str);
      auto connect_result = connection->connect();

      if (!connect_result) {
        GTEST_SKIP() << "PostgreSQL connection failed: " << connect_result.error().message
                     << ". Skipping PostgreSQL streaming tests.";
      }

      // Create test table and insert test data
      setup_test_data();

    } catch (const std::exception& e) {
      GTEST_SKIP() << "PostgreSQL setup failed: " << e.what()
                   << ". Skipping PostgreSQL streaming tests.";
    }
  }

  void TearDown() override {
    if (connection && connection->is_connected()) {
      // Clean up test data
      connection->execute_raw("DROP TABLE IF EXISTS users");
      connection->disconnect();
    }
  }

  void setup_test_data() {
    // Create users table
    auto create_result = connection->execute_raw(R"(
      CREATE TABLE IF NOT EXISTS users (
        id SERIAL PRIMARY KEY,
        name VARCHAR(100) NOT NULL,
        email VARCHAR(100),
        age INTEGER NOT NULL
      )
    )");
    ASSERT_TRUE(create_result) << "Failed to create users table: " << create_result.error().message;

    // Clear any existing data
    auto clear_result = connection->execute_raw("DELETE FROM users");
    ASSERT_TRUE(clear_result) << "Failed to clear users table: " << clear_result.error().message;

    // Insert test data (enough for streaming to be meaningful)
    for (int i = 1; i <= 1000; ++i) {
      auto insert_result = connection->execute_typed(
          "INSERT INTO users (name, email, age) VALUES (?, ?, ?)", "User" + std::to_string(i),
          "user" + std::to_string(i) + "@example.com",
          20 + (i % 50)  // Ages from 20 to 69
      );
      ASSERT_TRUE(insert_result) << "Failed to insert user " << i << ": "
                                 << insert_result.error().message;
    }
  }

  std::unique_ptr<connection::PostgreSQLConnection> connection;
};

TEST_F(PostgreSQLStreamingTest, BasicStreamingFunctionality) {
  if (!connection) GTEST_SKIP();

  // Create and initialize the streaming source
  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, email, age FROM users ORDER BY id LIMIT 10");
  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Failed to initialize streaming: " << init_result.error().message;

  // Create streaming result set
  auto streaming_result = result::StreamingResultSet(std::move(source));

  // Check that we can iterate through results
  int count = 0;
  for (const auto& lazy_row : streaming_result) {
    // Verify we can access row data
    auto id_result = lazy_row.get<int>(0);
    ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;
    EXPECT_EQ(*id_result, count + 1);

    auto name_result = lazy_row.get<std::string>(1);
    ASSERT_TRUE(name_result) << "Failed to get name: " << name_result.error().message;
    EXPECT_EQ(*name_result, "User" + std::to_string(count + 1));

    ++count;
  }

  EXPECT_EQ(count, 10);
}

TEST_F(PostgreSQLStreamingTest, StreamingWithParameters) {
  if (!connection) GTEST_SKIP();

  // Create and initialize the streaming source with parameters
  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, age FROM users WHERE age >= ? AND age <= ? ORDER BY id",
      {"25", "35"});
  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Failed to initialize streaming: " << init_result.error().message;

  // Create streaming result set
  auto streaming_result = result::StreamingResultSet(std::move(source));

  // Count results and verify age constraints
  int count = 0;
  for (const auto& lazy_row : streaming_result) {
    auto age_result = lazy_row.get<int>(2);
    ASSERT_TRUE(age_result) << "Failed to get age: " << age_result.error().message;
    EXPECT_GE(*age_result, 25);
    EXPECT_LE(*age_result, 35);
    ++count;
  }

  // We should have some results
  EXPECT_GT(count, 0);
}

TEST_F(PostgreSQLStreamingTest, StreamingWithSchemaIntegration) {
  if (!connection) GTEST_SKIP();

  Users users;

  // Create and initialize the streaming source
  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, email, age FROM users ORDER BY id LIMIT 5");
  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Failed to initialize streaming: " << init_result.error().message;

  // Create streaming result set
  auto streaming_result = result::StreamingResultSet(std::move(source));

  // Test accessing individual values (structured binding isn't directly supported with streaming)
  int count = 0;
  for (const auto& lazy_row : streaming_result) {
    auto id_result = lazy_row.get<int>(0);
    ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;
    EXPECT_EQ(*id_result, count + 1);

    auto name_result = lazy_row.get<std::string>(1);
    ASSERT_TRUE(name_result) << "Failed to get name: " << name_result.error().message;
    EXPECT_EQ(*name_result, "User" + std::to_string(count + 1));

    auto email_result = lazy_row.get<std::string>(2);
    ASSERT_TRUE(email_result) << "Failed to get email: " << email_result.error().message;
    EXPECT_EQ(*email_result, "user" + std::to_string(count + 1) + "@example.com");

    auto age_result = lazy_row.get<int>(3);
    ASSERT_TRUE(age_result) << "Failed to get age: " << age_result.error().message;
    EXPECT_GE(*age_result, 20);
    EXPECT_LE(*age_result, 69);

    ++count;
  }

  EXPECT_EQ(count, 5);
}

TEST_F(PostgreSQLStreamingTest, LargeResultSetStreaming) {
  if (!connection) GTEST_SKIP();

  // Create and initialize streaming source for all 1000 rows
  connection::PostgreSQLStreamingSource source(*connection,
                                               "SELECT id, name FROM users ORDER BY id");
  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Failed to initialize streaming: " << init_result.error().message;

  // Create streaming result set
  auto streaming_result = result::StreamingResultSet(std::move(source));

  // Process rows one by one
  int count = 0;
  int last_id = 0;

  for (const auto& lazy_row : streaming_result) {
    auto id_result = lazy_row.get<int>(0);
    ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;

    // Verify ordering
    EXPECT_GT(*id_result, last_id);
    last_id = *id_result;

    ++count;

    // Test early termination
    if (count >= 100) {
      break;
    }
  }

  EXPECT_EQ(count, 100);
}

TEST_F(PostgreSQLStreamingTest, StreamingEmptyResult) {
  if (!connection) GTEST_SKIP();

  // Create source for query that returns no results
  connection::PostgreSQLStreamingSource source(*connection,
                                               "SELECT id, name FROM users WHERE id > 10000");
  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Failed to initialize streaming: " << init_result.error().message;

  // Create streaming result set
  auto streaming_result = result::StreamingResultSet(std::move(source));

  // Should iterate zero times
  int count = 0;
  for (const auto& lazy_row : streaming_result) {
    (void)lazy_row;  // Avoid unused variable warning
    ++count;
  }

  EXPECT_EQ(count, 0);
}

TEST_F(PostgreSQLStreamingTest, StreamingErrorHandling) {
  if (!connection) GTEST_SKIP();

  // Try to stream from a non-existent table
  connection::PostgreSQLStreamingSource source(*connection, "SELECT * FROM non_existent_table");
  auto init_result = source.initialize();
  EXPECT_FALSE(init_result) << "Should have failed to initialize streaming from non-existent table";
}

TEST_F(PostgreSQLStreamingTest, StreamingWithNullValues) {
  if (!connection) GTEST_SKIP();

  // Insert a row with NULL values
  auto insert_result = connection->execute_raw(
      "INSERT INTO users (name, email, age) VALUES ('NullUser', NULL, 25)");
  ASSERT_TRUE(insert_result) << "Failed to insert user with NULL: "
                             << insert_result.error().message;

  // Create source to stream the row with NULL
  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT name, email FROM users WHERE name = 'NullUser'");
  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Failed to initialize streaming: " << init_result.error().message;

  // Create streaming result set
  auto streaming_result = result::StreamingResultSet(std::move(source));

  int count = 0;
  for (const auto& lazy_row : streaming_result) {
    auto name_result = lazy_row.get<std::string>(0);
    ASSERT_TRUE(name_result) << "Failed to get name: " << name_result.error().message;
    EXPECT_EQ(*name_result, "NullUser");

    auto email_result = lazy_row.get<std::optional<std::string>>(1);
    ASSERT_TRUE(email_result) << "Failed to get email: " << email_result.error().message;
    EXPECT_FALSE(email_result->has_value());  // Should be NULL

    ++count;
  }

  EXPECT_EQ(count, 1);
}

TEST_F(PostgreSQLStreamingTest, PerformanceComparison) {
  if (!connection) GTEST_SKIP();

  // Test performance difference between regular and streaming queries
  const std::string query = "SELECT id, name, email, age FROM users ORDER BY id";

  // Measure regular query execution
  auto start_regular = std::chrono::high_resolution_clock::now();
  auto regular_result = connection->execute_raw(query);
  auto end_regular = std::chrono::high_resolution_clock::now();

  ASSERT_TRUE(regular_result) << "Regular query failed: " << regular_result.error().message;

  // Measure streaming query initialization
  auto start_streaming = std::chrono::high_resolution_clock::now();
  connection::PostgreSQLStreamingSource source(*connection, query);
  auto init_result = source.initialize();
  auto end_streaming = std::chrono::high_resolution_clock::now();

  ASSERT_TRUE(init_result) << "Streaming initialization failed: " << init_result.error().message;

  // Calculate timing
  auto regular_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_regular - start_regular).count();
  auto streaming_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_streaming -
                                                                              start_streaming)
                            .count();

  // Streaming initialization should be faster than loading all data
  // This is especially true for large result sets
  std::cout << "Regular query time: " << regular_time << "ms" << std::endl;
  std::cout << "Streaming init time: " << streaming_time << "ms" << std::endl;

  // Verify both have the same number of rows when fully consumed
  auto streaming_result = result::StreamingResultSet(std::move(source));
  size_t streaming_count = 0;
  for (const auto& lazy_row : streaming_result) {
    (void)lazy_row;
    ++streaming_count;
  }

  EXPECT_EQ(regular_result->size(), streaming_count);
}

}  // namespace relx::test