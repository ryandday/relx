#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/connection/connection.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <relx/schema.hpp>

namespace {

// Mock connection implementation for testing
class MockConnection : public relx::Connection {
public:
  ~MockConnection() override = default;

  relx::ConnectionResult<void> connect() override { return {}; }
  relx::ConnectionResult<void> disconnect() override { return {}; }

  relx::ConnectionResult<relx::result::ResultSet> execute_raw(
      const std::string& sql, const std::vector<relx::bind_param>& params = {}) override {
    // Store the SQL and params for verification
    last_sql = sql;
    last_params = params;

    // Return the mock result set
    return mock_result_set;
  }

  bool is_connected() const override { return true; }

  relx::ConnectionResult<void> begin_transaction(
      relx::IsolationLevel isolation_level = relx::IsolationLevel::ReadCommitted) override {
    return {};
  }

  relx::ConnectionResult<void> commit_transaction() override { return {}; }
  relx::ConnectionResult<void> rollback_transaction() override { return {}; }
  bool in_transaction() const override { return false; }

  // Configure the mock to return specific results
  void set_mock_result_set(relx::result::ResultSet result_set) {
    mock_result_set = std::move(result_set);
  }

  // Access to last executed SQL and params for verification
  std::string last_sql;
  std::vector<relx::bind_param> last_params;
  relx::result::ResultSet mock_result_set;
};

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// Define a test table
struct [[=relx::table("users")]] Users {
  int id;
  std::string name;
  std::string email;
  int age;
  bool is_active;
  double score;
};
inline constexpr auto users = relx::t<Users>;

// clang-format on

// Define a DTO struct that matches some of the columns
struct UserDTO {
  int id;
  std::string name;
  int age;
};

// Define a DTO whose fields are declared in a different order than the select list.
// Mapping is name-matched, so the values still land in the right fields.
struct UserDTODifferentOrder {
  std::string name;
  int id;
  int age;
};

// DTO with a different number of fields
struct PartialUserDTO {
  int id;
  std::string name;
};

// DTO with all fields
struct CompleteUserDTO {
  int id;
  std::string name;
  std::string email;
  int age;
  bool is_active;
  double score;
};

class DtoMappingTest : public ::testing::Test {
protected:
  MockConnection conn;

  void SetUp() override {
    // Set up column names
    std::vector<std::string> column_names = {"id", "name", "age"};

    // Create cells for the rows
    std::vector<relx::result::Row> rows;

    // Row 1
    std::vector<relx::result::Cell> cells1;
    cells1.emplace_back("1");
    cells1.emplace_back("John Doe");
    cells1.emplace_back("30");
    rows.emplace_back(std::move(cells1), column_names);

    // Row 2
    std::vector<relx::result::Cell> cells2;
    cells2.emplace_back("2");
    cells2.emplace_back("Jane Smith");
    cells2.emplace_back("25");
    rows.emplace_back(std::move(cells2), column_names);

    // Row 3
    std::vector<relx::result::Cell> cells3;
    cells3.emplace_back("3");
    cells3.emplace_back("Bob Johnson");
    cells3.emplace_back("40");
    rows.emplace_back(std::move(cells3), column_names);

    // Create the result set
    auto sample_result = relx::result::ResultSet(std::move(rows), std::move(column_names));

    // Configure the mock connection to return our sample data
    conn.set_mock_result_set(std::move(sample_result));
  }
};

// Test basic struct mapping
TEST_F(DtoMappingTest, BasicStructMapping) {
  // Create a query that matches our sample data
  auto query = relx::query::select(users.id, users.name, users.age).from(users);

  // Execute the query with a return type
  auto result = conn.execute<UserDTO>(query);

  // Verify the result
  ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;

  // Check the mapped struct fields
  UserDTO user = *result;
  EXPECT_EQ(1, user.id);
  EXPECT_EQ("John Doe", user.name);
  EXPECT_EQ(30, user.age);

  // Verify the SQL was correct
  EXPECT_EQ(query.to_sql(), conn.last_sql);
}

// Test mapping to a struct whose fields are declared in a different order than the
// select list. Mapping is by column name, not position, so every field gets its own value.
TEST_F(DtoMappingTest, DifferentFieldOrder) {
  auto query = relx::query::select(users.id, users.name, users.age).from(users);

  auto result = conn.execute<UserDTODifferentOrder>(query);

  ASSERT_TRUE(result) << "Failed to execute query with different field order: "
                      << result.error().message;

  UserDTODifferentOrder user = *result;
  EXPECT_EQ("John Doe", user.name);
  EXPECT_EQ(1, user.id);
  EXPECT_EQ(30, user.age);
}

// Test mapping multiple rows
TEST_F(DtoMappingTest, MultipleRows) {
  auto query = relx::query::select(users.id, users.name, users.age).from(users);

  // Execute and get multiple rows
  auto result = conn.execute_many<UserDTO>(query);

  // Verify the result
  ASSERT_TRUE(result) << "Failed to execute_many query: " << result.error().message;

  // Should have three rows
  const auto& users_vector = *result;
  ASSERT_EQ(3, users_vector.size());

  // Check first user
  EXPECT_EQ(1, users_vector[0].id);
  EXPECT_EQ("John Doe", users_vector[0].name);
  EXPECT_EQ(30, users_vector[0].age);

  // Check second user
  EXPECT_EQ(2, users_vector[1].id);
  EXPECT_EQ("Jane Smith", users_vector[1].name);
  EXPECT_EQ(25, users_vector[1].age);

  // Check third user
  EXPECT_EQ(3, users_vector[2].id);
  EXPECT_EQ("Bob Johnson", users_vector[2].name);
  EXPECT_EQ(40, users_vector[2].age);
}

// Selecting a subset of a struct's fields is valid: unselected fields stay
// default-initialized. (Selecting a column with no matching field is a compile
// error via assert_struct_covers_select_list.)
TEST_F(DtoMappingTest, SubsetSelectLeavesExtraFieldsDefaulted) {
  std::vector<std::string> column_names = {"id", "name"};
  std::vector<relx::result::Row> rows;
  std::vector<relx::result::Cell> cells;
  cells.emplace_back("1");
  cells.emplace_back("John Doe");
  rows.emplace_back(std::move(cells), column_names);
  conn.set_mock_result_set(relx::result::ResultSet(std::move(rows), column_names));

  auto query = relx::query::select(users.id, users.name).from(users);

  // UserDTO has three fields (id, name, age); only two columns are selected
  auto result = conn.execute<UserDTO>(query);
  ASSERT_TRUE(result) << result.error().message;
  EXPECT_EQ(1, result->id);
  EXPECT_EQ("John Doe", result->name);
  EXPECT_EQ(0, result->age);  // not selected -> default-initialized
}

// Test empty result set
TEST_F(DtoMappingTest, EmptyResultSet) {
  // Configure the mock to return an empty result
  std::vector<std::string> column_names = {"id", "name", "age"};
  auto empty_result = relx::result::ResultSet({}, column_names);
  conn.set_mock_result_set(std::move(empty_result));

  auto query = relx::query::select(users.id, users.name, users.age).from(users);

  // Execute with empty result
  auto result = conn.execute<UserDTO>(query);

  // Should fail with "No results found"
  ASSERT_FALSE(result);
  EXPECT_EQ("No results found", result.error().message);

  // But execute_many should return an empty vector, not an error
  auto many_result = conn.execute_many<UserDTO>(query);
  ASSERT_TRUE(many_result);
  EXPECT_TRUE(many_result->empty());
}

// A raw query type without a compile-time select list: the static coverage check
// cannot fire, so an unconsumed result column must be caught at runtime
struct RawQuery {
  std::string to_sql() const { return "SELECT id, name, age, email, score FROM users"; }
  std::vector<relx::bind_param> bind_params() const { return {}; }
};

// Test that a result column with no matching field is a runtime mapping error
TEST_F(DtoMappingTest, UnconsumedResultColumnIsError) {
  // Set up result with more columns than the target struct
  std::vector<std::string> column_names = {"id", "name", "age", "email", "score"};

  std::vector<relx::result::Row> rows;
  std::vector<relx::result::Cell> cells;
  cells.emplace_back("1");
  cells.emplace_back("John Doe");
  cells.emplace_back("30");
  cells.emplace_back("john@example.com");
  cells.emplace_back("95.5");
  rows.emplace_back(std::move(cells), column_names);

  auto extra_result = relx::result::ResultSet(std::move(rows), std::move(column_names));
  conn.set_mock_result_set(std::move(extra_result));

  // UserDTO has no 'email'/'score' fields, so that data would be silently lost
  auto result = conn.execute<UserDTO>(RawQuery{});
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().message.find("result column 'email' has no matching field") !=
              std::string::npos)
      << result.error().message;
}

TEST_F(DtoMappingTest, DuplicateResultColumnIsError) {
  // SELECT u.id, p.id ... both result columns are named "id"; first-match-wins would
  // silently drop the second value, so the mapping must refuse
  std::vector<std::string> column_names = {"id", "id", "name"};

  std::vector<relx::result::Row> rows;
  std::vector<relx::result::Cell> cells;
  cells.emplace_back("1");
  cells.emplace_back("2");
  cells.emplace_back("John Doe");
  rows.emplace_back(std::move(cells), column_names);

  auto dup_result = relx::result::ResultSet(std::move(rows), std::move(column_names));
  conn.set_mock_result_set(std::move(dup_result));

  auto result = conn.execute<UserDTO>(RawQuery{});
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().message.find("duplicate result column 'id'") != std::string::npos)
      << result.error().message;
}

TEST(ConvertAndAssignBoolGrammar, OneGrammarEverywhere) {
  bool target = false;
  // The unified grammar: case-insensitive t/true/f/false only
  EXPECT_TRUE(relx::connection::convert_and_assign(target, std::string("t")).has_value());
  EXPECT_TRUE(target);
  EXPECT_TRUE(relx::connection::convert_and_assign(target, std::string("FALSE")).has_value());
  EXPECT_FALSE(target);

  // "1"/"yes" are not booleans in the struct-mapping path
  EXPECT_FALSE(relx::connection::convert_and_assign(target, std::string("1")).has_value());
  EXPECT_FALSE(relx::connection::convert_and_assign(target, std::string("yes")).has_value());
}

// Test type conversion errors
TEST_F(DtoMappingTest, TypeConversionErrors) {
  // Set up result with invalid data
  std::vector<std::string> column_names = {"id", "name", "age"};

  std::vector<relx::result::Row> rows;
  std::vector<relx::result::Cell> cells;
  cells.emplace_back("not_an_int");  // "id" has non-integer value
  cells.emplace_back("John Doe");
  cells.emplace_back("30");
  rows.emplace_back(std::move(cells), column_names);

  relx::result::ResultSet invalid_result(std::move(rows), std::move(column_names));
  conn.set_mock_result_set(std::move(invalid_result));

  auto query = relx::query::select(users.id, users.name, users.age).from(users);

  // This should fail with a conversion error
  auto result = conn.execute<UserDTO>(query);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().message.find("Failed to convert") != std::string::npos);
}

TEST(ConvertAndAssignStrictness, UnrecognizedBoolIsError) {
  bool target = true;
  auto ok = relx::connection::convert_and_assign(target, std::string("f"));
  ASSERT_TRUE(ok.has_value());
  EXPECT_FALSE(target);

  auto bad = relx::connection::convert_and_assign(target, std::string("maybe"));
  EXPECT_FALSE(bad.has_value());
}

TEST(ConvertAndAssignStrictness, PartialNumericParseIsError) {
  int target = 0;
  auto ok = relx::connection::convert_and_assign(target, std::string("12"));
  ASSERT_TRUE(ok.has_value());
  EXPECT_EQ(target, 12);

  auto bad = relx::connection::convert_and_assign(target, std::string("12abc"));
  EXPECT_FALSE(bad.has_value());
}

}  // namespace
