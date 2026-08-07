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

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// Test table schema for users
struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string email;
  int age;
};
inline constexpr auto users = relx::t<Users>;

// clang-format on

}  // namespace

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

      // Hard failure, not GTEST_SKIP: these are the only streaming regression tests,
      // and a skipped suite would turn CI green with streaming completely untested
      ASSERT_TRUE(connect_result) << "PostgreSQL connection failed: "
                                  << connect_result.error().message;

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

TEST_F(PostgreSQLStreamingTest, StreamingFromQueryObject) {
  if (!connection) GTEST_SKIP();

  // The query object carries both its SQL and its bound parameters
  auto query = relx::select(users.id, users.name, users.age)
                   .from(users)
                   .where(users.age >= 25 && users.age <= 35)
                   .order_by(users.id);

  auto streaming_result = connection::create_streaming_result(*connection, query);

  int count = 0;
  int last_id = 0;
  for (const auto& lazy_row : streaming_result) {
    auto id_result = lazy_row.get<int>("id");
    ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;
    EXPECT_GT(*id_result, last_id);
    last_id = *id_result;

    // Parameters from the query must have reached the server
    auto age_result = lazy_row.get<int>("age");
    ASSERT_TRUE(age_result) << "Failed to get age: " << age_result.error().message;
    EXPECT_GE(*age_result, 25);
    EXPECT_LE(*age_result, 35);

    ++count;
  }

  // Same predicate, executed non-streaming, must yield the same row count
  auto reference = connection->execute_typed(
      "SELECT COUNT(*) FROM users WHERE age >= ? AND age <= ?", 25, 35);
  ASSERT_TRUE(reference) << "Reference query failed: " << reference.error().message;
  auto expected_count = reference->at(0).get<int>(0);
  ASSERT_TRUE(expected_count) << "Failed to read count: " << expected_count.error().message;

  EXPECT_GT(count, 0);
  EXPECT_EQ(count, *expected_count);
  EXPECT_FALSE(streaming_result.last_error().has_value());
}

TEST_F(PostgreSQLStreamingTest, ForEachProcessesEveryRow) {
  if (!connection) GTEST_SKIP();

  // No explicit initialize(): the first row pull starts the query
  auto streaming_result = result::StreamingResultSet(connection::PostgreSQLStreamingSource(
      *connection, "SELECT id, name FROM users ORDER BY id LIMIT 10"));

  std::vector<int> ids;
  streaming_result.for_each([&ids](const auto& lazy_row) {
    auto id_result = lazy_row.template get<int>("id");
    ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;
    ids.push_back(*id_result);
  });

  ASSERT_EQ(ids.size(), 10);
  EXPECT_EQ(ids.front(), 1);
  EXPECT_EQ(ids.back(), 10);
}

TEST_F(PostgreSQLStreamingTest, ForEachStopsWhenCallbackReturnsTrue) {
  if (!connection) GTEST_SKIP();

  auto streaming_result = result::StreamingResultSet(
      connection::PostgreSQLStreamingSource(*connection, "SELECT id, name FROM users ORDER BY id"));

  int count = 0;
  streaming_result.for_each([&count](const auto& lazy_row) -> bool {
    auto id_result = lazy_row.template get<int>("id");
    EXPECT_TRUE(id_result);
    ++count;
    return count >= 5;  // true breaks the iteration
  });

  EXPECT_EQ(count, 5);
}

TEST_F(PostgreSQLStreamingTest, ManualIterationWithAdvanceAndIsAtEnd) {
  if (!connection) GTEST_SKIP();

  auto streaming_result = result::StreamingResultSet(connection::PostgreSQLStreamingSource(
      *connection, "SELECT id, name FROM users ORDER BY id LIMIT 3"));

  // begin() already sits on the first row, so advance() comes after processing it
  auto it = streaming_result.begin();
  int count = 0;
  while (!it.is_at_end()) {
    auto id_result = (*it).get<int>("id");
    ASSERT_TRUE(id_result) << "Failed to get id: " << id_result.error().message;
    EXPECT_EQ(*id_result, count + 1);
    ++count;
    it.advance();
  }

  EXPECT_EQ(count, 3);

  // Advancing past the end stays at the end
  it.advance();
  EXPECT_TRUE(it.is_at_end());
}

TEST_F(PostgreSQLStreamingTest, LastErrorDistinguishesFailureFromEmptyResult) {
  if (!connection) GTEST_SKIP();

  auto failed = result::StreamingResultSet(
      connection::PostgreSQLStreamingSource(*connection, "SELECT * FROM non_existent_table"));

  int failed_count = 0;
  for (const auto& lazy_row : failed) {
    (void)lazy_row;
    ++failed_count;
  }

  EXPECT_EQ(failed_count, 0);
  ASSERT_TRUE(failed.last_error().has_value()) << "A failed query must not look like an empty one";
  EXPECT_NE(failed.last_error()->message.find("non_existent_table"), std::string::npos);

  // An empty result set, by contrast, reports no error
  auto empty = result::StreamingResultSet(
      connection::PostgreSQLStreamingSource(*connection, "SELECT id FROM users WHERE id > 100000"));

  int empty_count = 0;
  for (const auto& lazy_row : empty) {
    (void)lazy_row;
    ++empty_count;
  }

  EXPECT_EQ(empty_count, 0);
  EXPECT_FALSE(empty.last_error().has_value());
}

TEST_F(PostgreSQLStreamingTest, StreamingWithSchemaIntegration) {
  if (!connection) GTEST_SKIP();

  // Table name comes from the schema rather than being hard-coded
  const std::string sql = "SELECT id, name, email, age FROM " + std::string(users.table_name) +
                          " ORDER BY id LIMIT 5";

  // Create and initialize the streaming source
  connection::PostgreSQLStreamingSource source(*connection, sql);
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

TEST_F(PostgreSQLStreamingTest, ByteaConversionOptIn) {
  if (!connection) GTEST_SKIP();

  auto setup = connection->execute_raw(
      "DROP TABLE IF EXISTS stream_bytea_test; "
      "CREATE TABLE stream_bytea_test (id INTEGER, payload BYTEA);");
  ASSERT_TRUE(setup) << setup.error().message;
  auto insert = connection->execute_raw(
      "INSERT INTO stream_bytea_test VALUES (1, '\\x48656c6c6f00ff'::bytea);");
  ASSERT_TRUE(insert) << insert.error().message;

  // Without opting in, the hex text passes through untouched
  {
    auto source = connection::PostgreSQLStreamingSource(*connection,
                                                        "SELECT payload FROM stream_bytea_test");
    auto streaming_result = result::StreamingResultSet(std::move(source));
    auto it = streaming_result.begin();
    ASSERT_FALSE(it.is_at_end());
    auto raw = (*it).get<std::string>("payload");
    ASSERT_TRUE(raw) << raw.error().message;
    EXPECT_EQ(*raw, "\\x48656c6c6f00ff");
  }

  // With set_convert_bytea(true) the cell carries the raw bytes
  {
    auto source = connection::PostgreSQLStreamingSource(*connection,
                                                        "SELECT payload FROM stream_bytea_test");
    source.set_convert_bytea(true);
    auto streaming_result = result::StreamingResultSet(std::move(source));
    auto it = streaming_result.begin();
    ASSERT_FALSE(it.is_at_end());
    auto decoded = (*it).get<std::string>("payload");
    ASSERT_TRUE(decoded) << decoded.error().message;
    const std::string expected{"Hello\x00\xff", 7};
    EXPECT_EQ(*decoded, expected);
  }

  auto cleanup = connection->execute_raw("DROP TABLE stream_bytea_test;");
  ASSERT_TRUE(cleanup) << cleanup.error().message;
}

}  // namespace relx::test