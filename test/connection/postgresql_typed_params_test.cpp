#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection/postgresql_connection.hpp>

namespace {

class PostgreSQLTypedParamsTest : public ::testing::Test {
protected:
  // Connection string for the Docker container
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";

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
      conn.execute_raw("DROP TABLE IF EXISTS typed_params_test");
      conn.disconnect();
    }
  }

  // Helper to create the test table
  void create_test_table(relx::connection::PostgreSQLConnection& conn) {
    std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS typed_params_test (
                id SERIAL PRIMARY KEY,
                int_val INTEGER,
                float_val FLOAT,
                text_val TEXT,
                bool_val BOOLEAN,
                nullable_val TEXT
            )
        )";
    auto result = conn.execute_raw(create_table_sql);
    ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
  }
};

TEST_F(PostgreSQLTypedParamsTest, TestBasicTypedParameters) {
  relx::connection::PostgreSQLConnection conn(conn_string);
  ASSERT_TRUE(conn.connect());

  // Create test table
  create_test_table(conn);

  // Insert test data using typed parameters
  auto result = conn.execute_typed("INSERT INTO typed_params_test (int_val, float_val, text_val, "
                                   "bool_val, nullable_val) VALUES (?, ?, ?, ?, ?)",
                                   42,               // int
                                   3.14159,          // float
                                   "Hello, world!",  // string
                                   true,             // bool
                                   nullptr           // nullptr for NULL
  );

  ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;

  // Verify the data was inserted correctly
  auto select_result = conn.execute_raw("SELECT * FROM typed_params_test");
  ASSERT_TRUE(select_result) << "Failed to select data: " << select_result.error().message;
  ASSERT_EQ(1, select_result->size());

  const auto& row = (*select_result)[0];

  auto int_val = row.get<int>("int_val");
  auto float_val = row.get<double>("float_val");
  auto text_val = row.get<std::string>("text_val");
  auto bool_val = row.get<bool>("bool_val");
  auto nullable_val = row.get<std::optional<std::string>>("nullable_val");

  ASSERT_TRUE(int_val);
  ASSERT_TRUE(float_val);
  ASSERT_TRUE(text_val);
  ASSERT_TRUE(bool_val);
  ASSERT_TRUE(nullable_val);

  EXPECT_EQ(42, *int_val);
  EXPECT_DOUBLE_EQ(3.14159, *float_val);
  EXPECT_EQ("Hello, world!", *text_val);
  EXPECT_TRUE(*bool_val);
  EXPECT_FALSE(nullable_val->has_value());

  // Clean up
  conn.disconnect();
}

TEST_F(PostgreSQLTypedParamsTest, TestMixedTypes) {
  relx::connection::PostgreSQLConnection conn(conn_string);
  ASSERT_TRUE(conn.connect());

  // Create test table
  create_test_table(conn);

  // Test different numeric types
  auto result = conn.execute_typed(
      "INSERT INTO typed_params_test (int_val, float_val, text_val, bool_val) VALUES (?, ?, ?, ?)",
      int8_t{8},                                  // tiny int
      float{2.71828f},                            // float
      std::string_view{"String view parameter"},  // string_view
      false                                       // bool false
  );

  ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;

  // Test with update
  auto update_result = conn.execute_typed(
      "UPDATE typed_params_test SET int_val = ?, text_val = ? WHERE bool_val = ?", 64,
      "Updated text", false);

  ASSERT_TRUE(update_result) << "Failed to update data: " << update_result.error().message;

  // Verify the data was updated correctly
  auto select_result = conn.execute_raw("SELECT * FROM typed_params_test WHERE bool_val = false");
  ASSERT_TRUE(select_result) << "Failed to select data: " << select_result.error().message;
  ASSERT_EQ(1, select_result->size());

  const auto& row = (*select_result)[0];

  auto int_val = row.get<int>("int_val");
  auto text_val = row.get<std::string>("text_val");

  ASSERT_TRUE(int_val);
  ASSERT_TRUE(text_val);

  EXPECT_EQ(64, *int_val);
  EXPECT_EQ("Updated text", *text_val);

  // Clean up
  conn.disconnect();
}

// Custom struct to test stream operator conversion
struct CustomType {
  int id;
  std::string name;

  friend std::ostream& operator<<(std::ostream& os, const CustomType& obj) {
    os << obj.id << ":" << obj.name;
    return os;
  }
};

TEST_F(PostgreSQLTypedParamsTest, TestCustomTypeConversion) {
  relx::connection::PostgreSQLConnection conn(conn_string);
  ASSERT_TRUE(conn.connect());

  // Create test table
  create_test_table(conn);

  // Test with custom type that has stream operator
  CustomType custom{100, "CustomObject"};

  auto result = conn.execute_typed(
      "INSERT INTO typed_params_test (int_val, text_val) VALUES (?, ?)", 42,
      custom  // Should convert using << operator
  );

  ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;

  // Verify the data was inserted correctly
  auto select_result = conn.execute_raw("SELECT * FROM typed_params_test");
  ASSERT_TRUE(select_result) << "Failed to select data: " << select_result.error().message;
  ASSERT_EQ(1, select_result->size());

  const auto& row = (*select_result)[0];
  auto text_val = row.get<std::string>("text_val");

  ASSERT_TRUE(text_val);
  EXPECT_EQ("100:CustomObject", *text_val);

  // Clean up
  conn.disconnect();
}

}  // namespace