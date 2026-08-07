#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/connection/postgresql_connection.hpp>
#include <relx/connection/sql_utils.hpp>

namespace {

class PostgreSQLBinaryTest : public ::testing::Test {
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
      conn.execute_raw("DROP TABLE IF EXISTS binary_test");
      conn.disconnect();
    }
  }

  // Helper to create the test table
  void create_test_table(relx::connection::PostgreSQLConnection& conn) {
    std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS binary_test (
                id SERIAL PRIMARY KEY,
                name TEXT,
                binary_data BYTEA
            )
        )";
    auto result = conn.execute_raw(create_table_sql);
    ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
  }

  // Helper to create binary data
  std::string create_binary_data(size_t size) {
    std::string data;
    data.reserve(size);

    for (size_t i = 0; i < size; ++i) {
      data.push_back(static_cast<char>(i % 256));
    }

    return data;
  }

  // Helper to validate binary data
  // We input as a string, but Postgres returns it as a hex string
  // So we need to convert it back to a binary blob
  bool validate_binary_data(const std::string& expected, const std::string& actual) {
    // Check if actual data is in hex format (PostgreSQL bytea hex format starts with \x)
    if (actual.size() >= 2 && actual.substr(0, 2) == "\\x") {
      // Convert hex string to binary for comparison
      std::string binary_actual;
      binary_actual.reserve((actual.size() - 2) / 2);

      for (size_t i = 2; i < actual.size(); i += 2) {
        if (i + 1 < actual.size()) {
          std::string hex_byte = actual.substr(i, 2);
          char byte = static_cast<char>(std::stoi(hex_byte, nullptr, 16));
          binary_actual.push_back(byte);
        }
      }

      if (expected.size() != binary_actual.size()) {
        std::cerr << "Size mismatch after hex conversion. Expected: " << expected.size()
                  << ", Actual: " << binary_actual.size() << std::endl;
        return false;
      }

      // Compare the binary content
      return std::memcmp(expected.data(), binary_actual.data(), expected.size()) == 0;
    }

    // If not in hex format, do direct comparison
    if (expected.size() != actual.size()) {
      return false;
    }
    return std::memcmp(expected.data(), actual.data(), expected.size()) == 0;
  }
};

TEST_F(PostgreSQLBinaryTest, TestBasicBinaryData) {
  relx::connection::PostgreSQLConnection conn(conn_string);
  ASSERT_TRUE(conn.connect());

  // Create test table
  create_test_table(conn);

  // Create binary data (1KB)
  std::string binary_data = create_binary_data(1024);

  // Insert binary data
  auto result = conn.execute_raw_binary(
      "INSERT INTO binary_test (name, binary_data) VALUES ($1, $2)", {"Test Binary", binary_data},
      {false, true}  // name is text, binary_data is binary
  );

  ASSERT_TRUE(result) << "Failed to insert binary data: " << result.error().message;

  // Verify the data was inserted correctly
  auto select_result = conn.execute_raw("SELECT * FROM binary_test");
  ASSERT_TRUE(select_result) << "Failed to select data: " << select_result.error().message;
  ASSERT_EQ(1, select_result->size());

  const auto& row = (*select_result)[0];

  auto name = row.get<std::string>("name");
  auto data = row.get<std::string>("binary_data");

  ASSERT_TRUE(name);
  ASSERT_TRUE(data);

  EXPECT_EQ("Test Binary", *name);
  EXPECT_TRUE(validate_binary_data(binary_data, *data));

  // Clean up
  conn.disconnect();
}

TEST_F(PostgreSQLBinaryTest, TestLargeBinaryData) {
  relx::connection::PostgreSQLConnection conn(conn_string);
  ASSERT_TRUE(conn.connect());

  // Create test table
  create_test_table(conn);

  // Create larger binary data (1MB)
  std::string large_binary_data = create_binary_data(1024 * 1024);

  // Insert binary data
  auto result = conn.execute_raw_binary(
      "INSERT INTO binary_test (name, binary_data) VALUES ($1, $2)",
      {"Large Binary", large_binary_data}, {false, true}  // name is text, binary_data is binary
  );

  ASSERT_TRUE(result) << "Failed to insert large binary data: " << result.error().message;

  // Verify the data was inserted correctly - use a parameterized query to avoid loading large data
  auto verify_result = conn.execute_raw(
      "SELECT id, name, LENGTH(binary_data) AS data_length FROM binary_test WHERE name = $1",
      {"Large Binary"});

  ASSERT_TRUE(verify_result) << "Failed to verify data: " << verify_result.error().message;
  ASSERT_EQ(1, verify_result->size());

  const auto& row = (*verify_result)[0];

  auto name = row.get<std::string>("name");
  auto length = row.get<int>("data_length");

  ASSERT_TRUE(name);
  ASSERT_TRUE(length);

  EXPECT_EQ("Large Binary", *name);
  EXPECT_EQ(1024 * 1024, *length);

  // Now retrieve the actual data to verify content
  auto data_result = conn.execute_raw(
      "SELECT binary_data FROM binary_test WHERE name = 'Large Binary'");
  ASSERT_TRUE(data_result) << "Failed to retrieve binary data: " << data_result.error().message;
  ASSERT_EQ(1, data_result->size());

  const auto& data_row = (*data_result)[0];
  auto data = data_row.get<std::string>(0);

  ASSERT_TRUE(data);
  EXPECT_TRUE(validate_binary_data(large_binary_data, *data));

  // Clean up
  conn.disconnect();
}

TEST_F(PostgreSQLBinaryTest, TestMixedBinaryAndTextParameters) {
  relx::connection::PostgreSQLConnection conn(conn_string);
  ASSERT_TRUE(conn.connect());

  // Create test table
  create_test_table(conn);

  // Create three different binary data blocks
  std::string binary_data1 = create_binary_data(512);
  std::string binary_data2 = create_binary_data(1024);
  std::string binary_data3 = create_binary_data(2048);

  // Test with multiple binary parameters
  auto multi_result = conn.execute_raw_binary(
      "INSERT INTO binary_test (name, binary_data) VALUES ($1, $2), ($3, $4), ($5, $6)",
      {"Item 1", binary_data1, "Item 2", binary_data2, "Item 3", binary_data3},
      {false, true, false, true, false, true}  // Alternating text and binary
  );

  ASSERT_TRUE(multi_result) << "Failed to insert multiple binary items: "
                            << multi_result.error().message;

  // Verify the data count
  auto count_result = conn.execute_raw("SELECT COUNT(*) FROM binary_test");
  ASSERT_TRUE(count_result) << "Failed to count records: " << count_result.error().message;
  ASSERT_EQ(1, count_result->size());

  const auto& count_row = (*count_result)[0];
  auto count = count_row.get<int>(0);

  ASSERT_TRUE(count);
  EXPECT_EQ(3, *count);

  // Verify each record
  auto records_result = conn.execute_raw("SELECT name, binary_data FROM binary_test ORDER BY id");
  ASSERT_TRUE(records_result) << "Failed to retrieve records: " << records_result.error().message;
  ASSERT_EQ(3, records_result->size());

  // Check first record
  auto name1 = records_result->at(0).get<std::string>("name");
  auto data1 = records_result->at(0).get<std::string>("binary_data");
  ASSERT_TRUE(name1);
  ASSERT_TRUE(data1);
  EXPECT_EQ("Item 1", *name1);
  EXPECT_TRUE(validate_binary_data(binary_data1, *data1));

  // Check second record
  auto name2 = records_result->at(1).get<std::string>("name");
  auto data2 = records_result->at(1).get<std::string>("binary_data");
  ASSERT_TRUE(name2);
  ASSERT_TRUE(data2);
  EXPECT_EQ("Item 2", *name2);
  EXPECT_TRUE(validate_binary_data(binary_data2, *data2));

  // Check third record
  auto name3 = records_result->at(2).get<std::string>("name");
  auto data3 = records_result->at(2).get<std::string>("binary_data");
  ASSERT_TRUE(name3);
  ASSERT_TRUE(data3);
  EXPECT_EQ("Item 3", *name3);
  EXPECT_TRUE(validate_binary_data(binary_data3, *data3));

  // Clean up
  conn.disconnect();
}

}  // namespace
// Server-free decoder tests: exact wire bytes in, canonical text out. These decoders
// (base-10000 NUMERIC math, UUID formatting, sign extension) were previously never
// executed by any test.
TEST(BinaryCellDecode, NegativeIntegersSignExtend) {
  using relx::connection::sql_utils::decode_binary_cell_for_testing;

  const char int2_neg[] = {'\xFF', '\xFF'};
  auto int2_result = decode_binary_cell_for_testing(21, int2_neg, 2);
  ASSERT_TRUE(int2_result) << int2_result.error();
  EXPECT_EQ(*int2_result, "-1");

  const char int8_neg[] = {'\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFE'};
  auto int8_result = decode_binary_cell_for_testing(20, int8_neg, 8);
  ASSERT_TRUE(int8_result) << int8_result.error();
  EXPECT_EQ(*int8_result, "-2");

  const char int4_neg[] = {'\x80', '\x00', '\x00', '\x00'};
  auto int4_result = decode_binary_cell_for_testing(23, int4_neg, 4);
  ASSERT_TRUE(int4_result) << int4_result.error();
  EXPECT_EQ(*int4_result, "-2147483648");
}

TEST(BinaryCellDecode, UuidFormatsCanonically) {
  using relx::connection::sql_utils::decode_binary_cell_for_testing;

  const char uuid_bytes[] = {'\x12', '\x34', '\x56', '\x78', '\x9a', '\xbc', '\xde', '\xf0',
                             '\x01', '\x23', '\x45', '\x67', '\x89', '\xab', '\xcd', '\xef'};
  auto uuid_result = decode_binary_cell_for_testing(2950, uuid_bytes, 16);
  ASSERT_TRUE(uuid_result) << uuid_result.error();
  EXPECT_EQ(*uuid_result, "12345678-9abc-def0-0123-456789abcdef");
}

TEST(BinaryCellDecode, NumericBase10000) {
  using relx::connection::sql_utils::decode_binary_cell_for_testing;

  // NUMERIC wire format: ndigits, weight, sign, dscale (int16 each), then base-10000
  // digits. 1234.5678 = digits [1234, 5678], weight 0, dscale 4.
  const auto make_numeric = [](std::vector<int> halves) {
    std::string bytes;
    for (const int h : halves) {
      bytes += static_cast<char>((h >> 8) & 0xFF);
      bytes += static_cast<char>(h & 0xFF);
    }
    return bytes;
  };

  const std::string positive = make_numeric({2, 0, 0x0000, 4, 1234, 5678});
  auto pos_result = decode_binary_cell_for_testing(1700, positive.data(),
                                                   static_cast<int>(positive.size()));
  ASSERT_TRUE(pos_result) << pos_result.error();
  EXPECT_EQ(*pos_result, "1234.5678");

  const std::string negative = make_numeric({2, 0, 0x4000, 4, 1234, 5678});
  auto neg_result = decode_binary_cell_for_testing(1700, negative.data(),
                                                   static_cast<int>(negative.size()));
  ASSERT_TRUE(neg_result) << neg_result.error();
  EXPECT_EQ(*neg_result, "-1234.5678");

  const std::string zero = make_numeric({0, 0, 0x0000, 0});
  auto zero_result = decode_binary_cell_for_testing(1700, zero.data(),
                                                    static_cast<int>(zero.size()));
  ASSERT_TRUE(zero_result) << zero_result.error();
  EXPECT_EQ(*zero_result, "0");
}
