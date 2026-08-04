#include <chrono>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>
#include <relx/connection/postgresql_connection.hpp>
#include <relx/connection/postgresql_streaming_source.hpp>
#include <relx/connection/streaming_errors.hpp>
#include <relx/results/streaming_result.hpp>

namespace relx::test {

/// @brief Test schema for streaming error handling tests
struct TestTable {
  schema::column<TestTable, "id", int> id;
  schema::column<TestTable, "name", std::string> name;
  schema::column<TestTable, "value", int> value;

  static constexpr std::string_view table_name = "streaming_error_test";
};

class StreamingErrorHandlingTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Get connection string from environment or use default
    std::string conn_string =
        std::getenv("RELX_TEST_CONNECTION_STRING")
            ? std::getenv("RELX_TEST_CONNECTION_STRING")
            : "host=localhost port=5432 dbname=relx_test user=postgres password=test_password";

    connection = std::make_unique<connection::PostgreSQLConnection>(conn_string);
    auto connect_result = connection->connect();

    if (!connect_result) {
      GTEST_SKIP() << "PostgreSQL connection failed: " << connect_result.error().message
                   << " (connection string: " << conn_string << ")";
      return;
    }

    try {
      setup_test_data();
    } catch (const std::exception& e) {
      GTEST_SKIP() << "PostgreSQL setup failed: " << e.what()
                   << " (connection string: " << conn_string << ")";
      return;
    }
  }

  void TearDown() override {
    if (connection && connection->is_connected()) {
      // Clean up test table
      connection->execute_raw("DROP TABLE IF EXISTS streaming_error_test");
      connection->disconnect();
    }
  }

  void setup_test_data() {
    // Drop and create test table
    auto drop_result = connection->execute_raw("DROP TABLE IF EXISTS streaming_error_test");
    ASSERT_TRUE(drop_result) << "Failed to drop test table: " << drop_result.error().message;

    auto create_result = connection->execute_raw(R"(
      CREATE TABLE streaming_error_test (
        id SERIAL PRIMARY KEY,
        name VARCHAR(100) NOT NULL,
        value INTEGER NOT NULL
      )
    )");
    ASSERT_TRUE(create_result) << "Failed to create test table: " << create_result.error().message;

    // Insert test data
    for (int i = 1; i <= 100; ++i) {
      auto insert_result = connection->execute_raw(
          "INSERT INTO streaming_error_test (name, value) VALUES (?, ?)",
          {"Name" + std::to_string(i), std::to_string(i * 10)});
      ASSERT_TRUE(insert_result) << "Failed to insert test data " << i << ": "
                                 << insert_result.error().message;
    }
  }

  std::unique_ptr<connection::PostgreSQLConnection> connection;
};

TEST_F(StreamingErrorHandlingTest, InitializationErrorHandling) {
  if (!connection) GTEST_SKIP();

  // Test 1: Invalid table name
  {
    connection::PostgreSQLStreamingSource source(*connection, "SELECT * FROM non_existent_table");
    auto init_result = source.initialize();

    EXPECT_FALSE(init_result) << "Should have failed for non-existent table";

    if (!init_result) {
      const auto& error = init_result.error();
      EXPECT_EQ(error.error_type, connection::StreamingErrorType::QueryExecutionFailed);
      EXPECT_FALSE(error.message.empty());
      EXPECT_FALSE(error.is_recoverable());

      // Check formatted message contains useful information
      std::string formatted = error.formatted_message();
      EXPECT_TRUE(formatted.find("non_existent_table") != std::string::npos ||
                  formatted.find("relation") != std::string::npos);

      std::cout << "Initialization error (non-existent table): " << formatted << '\n';
    }
  }

  // Test 2: Invalid SQL syntax
  {
    connection::PostgreSQLStreamingSource source(*connection, "INVALID SQL SYNTAX HERE");
    auto init_result = source.initialize();

    EXPECT_FALSE(init_result) << "Should have failed for invalid SQL";

    if (!init_result) {
      const auto& error = init_result.error();
      EXPECT_EQ(error.error_type, connection::StreamingErrorType::QueryExecutionFailed);
      EXPECT_FALSE(error.message.empty());
      EXPECT_FALSE(error.is_recoverable());

      std::cout << "Initialization error (invalid SQL): " << error.formatted_message() << '\n';
    }
  }

  // Test 3: SQL with wrong parameter count
  {
    connection::PostgreSQLStreamingSource source(
        *connection, "SELECT * FROM streaming_error_test WHERE id = ? AND name = ?",
        {"1"});  // Only one parameter, but query expects two
    auto init_result = source.initialize();

    EXPECT_FALSE(init_result) << "Should have failed for parameter count mismatch";

    if (!init_result) {
      const auto& error = init_result.error();
      EXPECT_EQ(error.error_type, connection::StreamingErrorType::QueryExecutionFailed);
      std::cout << "Initialization error (parameter mismatch): " << error.formatted_message()
                << '\n';
    }
  }
}

TEST_F(StreamingErrorHandlingTest, SuccessfulStreamingWithErrorHelpers) {
  if (!connection) GTEST_SKIP();

  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, value FROM streaming_error_test ORDER BY id LIMIT 5");

  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Initialization should succeed: "
                           << init_result.error().formatted_message();

  int row_count = 0;
  while (true) {
    auto row_result = source.get_next_row();

    // Test the helper functions
    if (connection::is_end_of_stream(row_result)) {
      std::cout << "Reached end of stream normally" << '\n';
      break;
    }

    if (connection::is_error(row_result)) {
      FAIL() << "Unexpected error during streaming: " << row_result.error().formatted_message();
    }

    ASSERT_TRUE(connection::has_row_data(row_result)) << "Should have row data";

    const std::string& row_data = connection::get_row_data(row_result);
    EXPECT_FALSE(row_data.empty());

    ++row_count;
    std::cout << "Row " << row_count << ": " << row_data << '\n';
  }

  EXPECT_EQ(row_count, 5) << "Should have streamed exactly 5 rows";
}

TEST_F(StreamingErrorHandlingTest, ConnectionLossSimulation) {
  if (!connection) GTEST_SKIP();

  // This test demonstrates how connection loss would be handled
  // In a real scenario, connection loss would be detected during streaming

  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, value FROM streaming_error_test ORDER BY id");

  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Initialization should succeed";

  // Simulate reading a few rows successfully
  int rows_read = 0;
  for (int i = 0; i < 3; ++i) {
    auto row_result = source.get_next_row();

    if (connection::is_end_of_stream(row_result)) {
      break;  // Fewer rows than expected, but that's ok for this test
    }

    if (connection::is_error(row_result)) {
      const auto& error = row_result.error();
      if (error.error_type == connection::StreamingErrorType::ConnectionLost) {
        std::cout << "Connection lost detected: " << error.formatted_message() << '\n';
        EXPECT_TRUE(error.is_recoverable()) << "Connection loss should be recoverable";
        break;
      } else {
        FAIL() << "Unexpected error type: " << error.formatted_message();
      }
    }

    ASSERT_TRUE(connection::has_row_data(row_result));
    ++rows_read;
  }

  std::cout << "Successfully read " << rows_read << " rows before test completion" << '\n';
}

TEST_F(StreamingErrorHandlingTest, EmptyResultSetHandling) {
  if (!connection) GTEST_SKIP();

  // Test streaming from a query that returns no results
  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, value FROM streaming_error_test WHERE id > 1000");

  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Initialization should succeed even for empty result set";

  // Should immediately get end-of-stream
  auto row_result = source.get_next_row();
  EXPECT_TRUE(connection::is_end_of_stream(row_result))
      << "Should indicate end of stream for empty result";
  EXPECT_FALSE(connection::is_error(row_result)) << "Empty result should not be an error";
  EXPECT_FALSE(connection::has_row_data(row_result)) << "Should not have row data";

  std::cout << "Empty result set handled correctly" << '\n';
}

TEST_F(StreamingErrorHandlingTest, ErrorRecoverabilityClassification) {
  if (!connection) GTEST_SKIP();

  // Test different error types and their recoverability
  struct ErrorTestCase {
    std::string name;
    std::string sql;
    std::vector<std::string> params;
    connection::StreamingErrorType expected_type;
    bool expected_recoverable;
  };

  std::vector<ErrorTestCase> test_cases = {
      {"Non-existent table",
       "SELECT * FROM non_existent_table_xyz",
       {},
       connection::StreamingErrorType::QueryExecutionFailed,
       false},
      {"Invalid syntax",
       "SELET * FOM streaming_error_test",  // Intentional typos
       {},
       connection::StreamingErrorType::QueryExecutionFailed,
       false},
      {"Type mismatch",
       "SELECT * FROM streaming_error_test WHERE id = ?",
       {"not_a_number"},
       connection::StreamingErrorType::QueryExecutionFailed,
       false}};

  for (const auto& test_case : test_cases) {
    std::cout << "\nTesting error case: " << test_case.name << '\n';

    connection::PostgreSQLStreamingSource source(*connection, test_case.sql, test_case.params);
    auto init_result = source.initialize();

    EXPECT_FALSE(init_result) << "Should fail for: " << test_case.name;

    if (!init_result) {
      const auto& error = init_result.error();
      EXPECT_EQ(error.error_type, test_case.expected_type)
          << "Error type mismatch for: " << test_case.name;
      EXPECT_EQ(error.is_recoverable(), test_case.expected_recoverable)
          << "Recoverability mismatch for: " << test_case.name;

      std::cout << "  Error: " << error.formatted_message() << '\n';
      std::cout << "  Recoverable: " << (error.is_recoverable() ? "Yes" : "No") << '\n';
    }
  }
}

TEST_F(StreamingErrorHandlingTest, StreamingResultSetIntegration) {
  if (!connection) GTEST_SKIP();

  // Test the streaming result set with improved error handling
  connection::PostgreSQLStreamingSource source(
      *connection, "SELECT id, name, value FROM streaming_error_test ORDER BY id LIMIT 10");

  auto init_result = source.initialize();
  ASSERT_TRUE(init_result) << "Initialization should succeed: "
                           << init_result.error().formatted_message();

  auto streaming_result = result::StreamingResultSet(std::move(source));

  int row_count = 0;
  try {
    for (const auto& lazy_row : streaming_result) {
      auto id_result = lazy_row.get<int>(0);
      ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;

      auto name_result = lazy_row.get<std::string>(1);
      ASSERT_TRUE(name_result) << "Failed to get name: " << name_result.error().message;

      auto value_result = lazy_row.get<int>(2);
      ASSERT_TRUE(value_result) << "Failed to get value: " << value_result.error().message;

      EXPECT_EQ(*id_result, row_count + 1);
      EXPECT_EQ(*name_result, "Name" + std::to_string(row_count + 1));
      EXPECT_EQ(*value_result, (row_count + 1) * 10);

      ++row_count;
    }
  } catch (const std::exception& e) {
    FAIL() << "Exception during streaming: " << e.what();
  }

  EXPECT_EQ(row_count, 10) << "Should have processed exactly 10 rows";
  std::cout << "Successfully streamed " << row_count << " rows with error handling integration"
            << '\n';
}

TEST_F(StreamingErrorHandlingTest, ErrorFormattingAndDetails) {
  if (!connection) GTEST_SKIP();

  // Test detailed error formatting
  connection::PostgreSQLStreamingSource source(*connection, "SELECT * FROM does_not_exist");
  auto init_result = source.initialize();

  ASSERT_FALSE(init_result) << "Should fail for non-existent table";

  const auto& error = init_result.error();

  // Test basic properties
  EXPECT_FALSE(error.message.empty());
  EXPECT_EQ(error.error_type, connection::StreamingErrorType::QueryExecutionFailed);

  // Test formatted message
  std::string formatted = error.formatted_message();
  EXPECT_FALSE(formatted.empty());
  EXPECT_TRUE(formatted.length() >=
              error.message.length());  // Should be at least as long as base message

  std::cout << "Detailed error formatting test:" << '\n';
  std::cout << "  Basic message: " << error.message << '\n';
  std::cout << "  Formatted message: " << formatted << '\n';
  std::cout << "  Error code: " << error.error_code << '\n';
  std::cout << "  Is recoverable: " << (error.is_recoverable() ? "Yes" : "No") << '\n';

  if (error.sql_state) {
    std::cout << "  SQL State: " << *error.sql_state << '\n';
  }

  if (error.detail) {
    std::cout << "  Detail: " << *error.detail << '\n';
  }

  if (error.hint) {
    std::cout << "  Hint: " << *error.hint << '\n';
  }
}

}  // namespace relx::test