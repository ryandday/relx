#include <gtest/gtest.h>
#include "relx/results/result.hpp"
#include "relx/query/core.hpp"
#include "relx/query/select.hpp"
#include "relx/schema/table.hpp"
#include "relx/schema/column.hpp"
#include <chrono>

using namespace relx;

// Table schema that matches our test data
struct Users {
  static constexpr std::string_view table_name = "users";
  
  schema::column<Users, "id", int> id;
  schema::column<Users, "name", std::string> name;
  schema::column<Users, "email", std::string> email;
  schema::column<Users, "age", int> age;
};

class LazyParsingTest : public ::testing::Test {
protected:
  void SetUp() override {
    raw_data_ = 
      "id|name|email|age\n"
      "1|John Doe|john@example.com|30\n"
      "2|Jane Smith|jane@example.com|25\n"
      "3|Bob Johnson|bob@example.com|35\n";
  }

  std::string raw_data_;
  
  // Helper to create the query
  auto create_query() {
    Users users;
    return query::select(&Users::id, &Users::name, &Users::email, &Users::age).from(users);
  }
};

TEST_F(LazyParsingTest, LazyResultSetBasicFunctionality) {
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(raw_data_));
  
  // Test size - should trigger row boundary parsing
  EXPECT_EQ(lazy_result.size(), 3);
  EXPECT_FALSE(lazy_result.empty());
  
  // Test column names
  auto column_names = lazy_result.column_names();
  EXPECT_EQ(column_names.size(), 4);
  EXPECT_EQ(column_names[0], "id");
  EXPECT_EQ(column_names[1], "name");
  EXPECT_EQ(column_names[2], "email");
  EXPECT_EQ(column_names[3], "age");
}

TEST_F(LazyParsingTest, LazyRowAccess) {
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(raw_data_));
  
  // Access first row
  auto first_row = lazy_result[0];
  EXPECT_EQ(first_row.size(), 4);
  
  // Test typed access
  auto id = first_row.get<int>(0);
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(*id, 1);
  
  auto name = first_row.get<std::string>(1);
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "John Doe");
  
  // Test column name access
  auto email = first_row.get<std::string>("email");
  ASSERT_TRUE(email.has_value());
  EXPECT_EQ(*email, "john@example.com");
  
  auto age = first_row.get<int>("age");
  ASSERT_TRUE(age.has_value());
  EXPECT_EQ(*age, 30);
}

TEST_F(LazyParsingTest, LazyCellParsing) {
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(raw_data_));
  auto first_row = lazy_result[0];
  
  // Get lazy cell
  auto cell_result = first_row.get_cell(1);
  ASSERT_TRUE(cell_result.has_value());
  
  auto lazy_cell = *cell_result;
  
  // Test raw value access
  EXPECT_EQ(lazy_cell.get_raw_value(), "John Doe");
  EXPECT_FALSE(lazy_cell.is_null());
  
  // Test typed conversion
  auto as_string = lazy_cell.as<std::string>();
  ASSERT_TRUE(as_string.has_value());
  EXPECT_EQ(*as_string, "John Doe");
}

TEST_F(LazyParsingTest, LazyResultSetIteration) {
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(raw_data_));
  
  std::vector<std::string> names;
  for (const auto& row : lazy_result) {
    auto name = row.get<std::string>("name");
    if (name) {
      names.push_back(*name);
    }
  }
  
  EXPECT_EQ(names.size(), 3);
  EXPECT_EQ(names[0], "John Doe");
  EXPECT_EQ(names[1], "Jane Smith");
  EXPECT_EQ(names[2], "Bob Johnson");
}

TEST_F(LazyParsingTest, ConversionToRegularResultSet) {
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(raw_data_));
  auto regular_result = lazy_result.to_result_set();
  
  EXPECT_EQ(regular_result.size(), 3);
  EXPECT_EQ(regular_result.column_count(), 4);
  
  // Test that data is correctly converted
  auto first_row = regular_result[0];
  auto name = first_row.get<std::string>("name");
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "John Doe");
}

TEST_F(LazyParsingTest, ErrorHandling) {
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(raw_data_));
  
  // Test out of bounds access
  EXPECT_THROW(lazy_result.at(10), std::out_of_range);
  
  // Test invalid column access
  auto first_row = lazy_result[0];
  auto invalid_cell = first_row.get_cell("nonexistent");
  EXPECT_FALSE(invalid_cell.has_value());
  
  auto invalid_value = first_row.get<std::string>("nonexistent");
  EXPECT_FALSE(invalid_value.has_value());
}

TEST_F(LazyParsingTest, TypeConversionErrors) {
  std::string test_data = 
    "id|value\n"
    "1|not_a_number\n";
    
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::move(test_data));
  auto first_row = lazy_result[0];
  
  // Should fail to convert "not_a_number" to int
  auto invalid_int = first_row.get<int>("value");
  EXPECT_FALSE(invalid_int.has_value());
  
  // Should succeed to get as string
  auto as_string = first_row.get<std::string>("value");
  ASSERT_TRUE(as_string.has_value());
  EXPECT_EQ(*as_string, "not_a_number");
}

TEST_F(LazyParsingTest, BooleanConversion) {
  std::string test_data = 
    "bool_col|numeric_col\n"
    "true|1\n"
    "false|0\n";
    
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::move(test_data));
  
  auto first_row = lazy_result[0];
  
  // Test explicit boolean string
  auto bool_val = first_row.get<bool>("bool_col");
  ASSERT_TRUE(bool_val.has_value());
  EXPECT_TRUE(*bool_val);
  
  // Test numeric boolean conversion (should require explicit flag)
  auto numeric_as_bool_strict = first_row.get<bool>("numeric_col", false);
  EXPECT_FALSE(numeric_as_bool_strict.has_value()); // Should fail without allow_numeric_bools
  
  auto numeric_as_bool_allowed = first_row.get<bool>("numeric_col", true);
  ASSERT_TRUE(numeric_as_bool_allowed.has_value());
  EXPECT_TRUE(*numeric_as_bool_allowed); // 1 should convert to true
}

TEST_F(LazyParsingTest, NullHandling) {
  std::string test_data = 
    "id|nullable_col\n"
    "1|NULL\n"
    "2|valid_value\n";
    
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::move(test_data));
  
  // Test NULL value
  auto first_row = lazy_result[0];
  auto cell = first_row.get_cell("nullable_col");
  ASSERT_TRUE(cell.has_value());
  EXPECT_TRUE(cell->is_null());
  
  // Should fail to convert NULL to non-optional type
  auto as_string = first_row.get<std::string>("nullable_col");
  EXPECT_FALSE(as_string.has_value());
  
  // Should succeed to convert NULL to optional type
  auto as_optional = first_row.get<std::optional<std::string>>("nullable_col");
  ASSERT_TRUE(as_optional.has_value());
  EXPECT_FALSE(as_optional->has_value()); // Should be std::nullopt
  
  // Test non-NULL value in second row
  auto second_row = lazy_result[1];
  auto non_null_cell = second_row.get_cell("nullable_col");
  ASSERT_TRUE(non_null_cell.has_value());
  EXPECT_FALSE(non_null_cell->is_null());
  
  auto non_null_value = second_row.get<std::string>("nullable_col");
  ASSERT_TRUE(non_null_value.has_value());
  EXPECT_EQ(*non_null_value, "valid_value");
}

// Mock data source for streaming tests
class MockDataSource {
public:
  MockDataSource() {
    column_names_ = {"id", "name"};
    data_ = {"1|Alice", "2|Bob", "3|Charlie"};
    current_index_ = 0;
  }

  std::optional<std::string> get_next_row() {
    if (current_index_ >= data_.size()) {
      return std::nullopt;
    }
    auto result = data_[current_index_++];
    return result;
  }

  const std::vector<std::string>& get_column_names() const {
    return column_names_;
  }

private:
  std::vector<std::string> data_;
  std::vector<std::string> column_names_;
  size_t current_index_;
};

TEST_F(LazyParsingTest, StreamingResultSet) {
  MockDataSource source;
  result::StreamingResultSet streaming_result(std::move(source));
  
  std::vector<std::string> names;
  size_t count = 0;
  
  for (const auto& row : streaming_result) {
    ++count;
    auto name = row.get<std::string>("name");
    if (name) {
      names.push_back(*name);
    }
  }
  
  EXPECT_EQ(count, 3);
  ASSERT_EQ(names.size(), 3);
  EXPECT_EQ(names[0], "Alice");
  EXPECT_EQ(names[1], "Bob");
  EXPECT_EQ(names[2], "Charlie");
}

TEST_F(LazyParsingTest, PerformanceComparison) {
  // Create a larger dataset for performance testing
  std::string large_data = "id|name|email|age\n";
  for (int i = 1; i <= 1000; ++i) {
    large_data += std::to_string(i) + "|User" + std::to_string(i) + 
                  "|user" + std::to_string(i) + "@example.com|" + 
                  std::to_string(20 + (i % 50)) + "\n";
  }
  
  auto query = create_query();
  
  // Test eager parsing
  auto start_eager = std::chrono::high_resolution_clock::now();
  auto eager_result = result::parse(query, large_data);
  auto end_eager = std::chrono::high_resolution_clock::now();
  
  // Test lazy parsing
  auto start_lazy = std::chrono::high_resolution_clock::now();
  auto lazy_result = result::parse_lazy(query, std::string(large_data));
  auto end_lazy = std::chrono::high_resolution_clock::now();
  
  auto eager_time = std::chrono::duration_cast<std::chrono::microseconds>(end_eager - start_eager);
  auto lazy_time = std::chrono::duration_cast<std::chrono::microseconds>(end_lazy - start_lazy);
  
  // Lazy parsing should be significantly faster for initial setup
  EXPECT_LT(lazy_time.count(), eager_time.count());
  
  // Both should produce the same number of rows
  ASSERT_TRUE(eager_result.has_value());
  EXPECT_EQ(eager_result->size(), lazy_result.size());
  EXPECT_EQ(eager_result->size(), 1000);
}

TEST_F(LazyParsingTest, MemoryUsageComparison) {
  // This test demonstrates the memory efficiency concept
  // In practice, lazy parsing uses less memory by not storing all parsed cells
  
  std::string data = "id|data\n";
  for (int i = 1; i <= 100; ++i) {
    // Create rows with large string data
    data += std::to_string(i) + "|" + std::string(1000, 'A' + (i % 26)) + "\n";
  }
  
  // Lazy parsing - only stores the raw string until accessed
  auto query = create_query();
  auto lazy_result = result::parse_lazy(query, std::string(data));
  EXPECT_EQ(lazy_result.size(), 100);
  
  // Access only a few rows to demonstrate selective parsing
  auto first_row = lazy_result[0];
  auto data_cell = first_row.get<std::string>("data");
  ASSERT_TRUE(data_cell.has_value());
  EXPECT_EQ(data_cell->size(), 1000);
  EXPECT_EQ((*data_cell)[0], 'B'); // First row (i=1): 'A' + (1 % 26) = 'B'
  
  // The remaining 99 rows' data cells haven't been parsed yet
  // This saves memory compared to eager parsing which would parse everything
} 