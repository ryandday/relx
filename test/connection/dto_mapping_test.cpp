#include <gtest/gtest.h>
#include <relx/connection/connection.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <string>
#include <vector>
#include <optional>
#include <boost/pfr.hpp>

// Mock connection implementation for testing
class MockConnection : public relx::Connection {
public:
    ~MockConnection() override = default;
    
    relx::ConnectionResult<void> connect() override { return {}; }
    relx::ConnectionResult<void> disconnect() override { return {}; }
    
    relx::ConnectionResult<relx::result::ResultSet> execute_raw(
        const std::string& sql, 
        const std::vector<std::string>& params = {}) override {
        
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
    std::vector<std::string> last_params;
    relx::result::ResultSet mock_result_set;
};

// Define a test table
struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<Users, "id", int> id;
    relx::schema::column<Users, "name", std::string> name;
    relx::schema::column<Users, "email", std::string> email;
    relx::schema::column<Users, "age", int> age;
    relx::schema::column<Users, "is_active", bool> is_active;
    relx::schema::column<Users, "score", double> score;
};

// Define a DTO struct that matches some of the columns
struct UserDTO {
    int id;
    std::string name;
    int age;
};

// Define a DTO with different field order
// Will not work until we have true reflection support in C++26
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
    Users users;
    
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

// Test basic struct mapping with Boost PFR
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

// Test mapping to a struct with different field order
// This will not work until we have true reflection support
// TEST_F(PfrMappingTest, DifferentFieldOrder) {
//     auto query = relx::query::select(users.id, users.name, users.age).from(users);
    
//     // Execute with a differently ordered struct
//     auto result = conn.execute<UserDTODifferentOrder>(query);
    
//     // This should still work because we map by position, not name
//     ASSERT_TRUE(result) << "Failed to execute query with different field order: " << result.error().message;
    
//     // In this case, the fields will be mapped incorrectly due to position mismatch
//     UserDTODifferentOrder user = *result;
//     EXPECT_EQ("1", user.name);        // Should get the ID value as a string
//     EXPECT_EQ(1, user.id);            // Should get name value parsed as int (will fail)
//     EXPECT_EQ(30, user.age);          // This one is correct by coincidence
// }

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

// Test field count mismatch error
TEST_F(DtoMappingTest, FieldCountMismatch) {
    auto query = relx::query::select(users.id, users.name, users.age).from(users);
    
    // Try to map to a struct with fewer fields
    auto result = conn.execute<PartialUserDTO>(query);
    
    // This should fail
    ASSERT_FALSE(result);
    // Find the string in the error message
    EXPECT_TRUE(result.error().message.find("Column count does not match struct field count, 3 != 2") != std::string::npos);
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

// Test with more columns than needed
TEST_F(DtoMappingTest, ExtraColumnsInResultSet) {
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
    
    auto query = relx::query::select(users.id, users.name, users.age, users.email, users.score).from(users);
    
    // This should fail because we have too many columns
    auto result = conn.execute<UserDTO>(query);
    ASSERT_FALSE(result);
    // Find the string in the error message
    EXPECT_TRUE(result.error().message.find("Column count does not match struct field count, 5 != 3") != std::string::npos);
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