#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <array>

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

// Test fixture for result processing tests
class ResultTest : public ::testing::Test {
protected:
    Users users;
    std::string raw_results_;
    
    // Define the type of the query
    using QueryType = decltype(relx::query::select(
        std::declval<Users>().id,
        std::declval<Users>().name,
        std::declval<Users>().email,
        std::declval<Users>().age,
        std::declval<Users>().is_active,
        std::declval<Users>().score
    ).from(std::declval<Users>()));
    
    // Initialize the query directly in the constructor
    QueryType query_ = relx::query::select(
        users.id, users.name, users.email, users.age, users.is_active, users.score
    ).from(users);

    void SetUp() override {
        // Create a sample raw result string (pipe-separated values)
        raw_results_ = 
            "id|name|email|age|is_active|score\n"
            "1|John Doe|john@example.com|30|1|95.5\n"
            "2|Jane Smith|jane@example.com|28|1|92.3\n"
            "3|Bob Johnson|bob@example.com|35|0|85.7\n";
    }
};

// Test parsing basic results
TEST_F(ResultTest, BasicParsing) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(3, results.size());
    EXPECT_EQ(6, results.column_names().size());
    
    EXPECT_EQ("id", results.column_names()[0]);
    EXPECT_EQ("name", results.column_names()[1]);
    EXPECT_EQ("email", results.column_names()[2]);
    EXPECT_EQ("age", results.column_names()[3]);
    EXPECT_EQ("is_active", results.column_names()[4]);
    EXPECT_EQ("score", results.column_names()[5]);
}

// Test accessing values by index
TEST_F(ResultTest, AccessByIndex) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    ASSERT_GT(results.size(), 0);
    
    const auto& first_row = results.at(0);
    
    // Get by index
    auto id = first_row.get<int>(0);
    ASSERT_TRUE(id) << id.error().message;
    EXPECT_EQ(1, *id);
    
    auto name = first_row.get<std::string>(1);
    ASSERT_TRUE(name) << name.error().message;
    EXPECT_EQ("John Doe", *name);
    
    // Test error case - wrong index
    auto out_of_bounds = first_row.get<int>(10);
    EXPECT_FALSE(out_of_bounds);
    
    // Test error case - wrong type
    auto wrong_type = first_row.get<bool>(0);
    EXPECT_FALSE(wrong_type);
}

// Test accessing values by column name
TEST_F(ResultTest, AccessByName) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    ASSERT_GT(results.size(), 0);
    
    const auto& first_row = results.at(0);
    
    // Get by name
    auto id = first_row.get<int>("id");
    ASSERT_TRUE(id) << id.error().message;
    EXPECT_EQ(1, *id);
    
    auto name = first_row.get<std::string>("name");
    ASSERT_TRUE(name) << name.error().message;
    EXPECT_EQ("John Doe", *name);
    
    // Test error case - wrong name
    auto not_found = first_row.get<int>("not_a_column");
    EXPECT_FALSE(not_found);
    
    // Test error case - wrong type
    auto wrong_type = first_row.get<bool>("id");
    EXPECT_FALSE(wrong_type);
}

// Test accessing values via column objects
TEST_F(ResultTest, AccessByColumn) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    ASSERT_GT(results.size(), 0);
    
    const auto& first_row = results.at(0);
    
    // Use the table's column objects directly
    auto id = first_row.get<int>(users.id);
    ASSERT_TRUE(id) << id.error().message;
    EXPECT_EQ(1, *id);
    
    auto name = first_row.get<std::string>(users.name);
    ASSERT_TRUE(name) << name.error().message;
    EXPECT_EQ("John Doe", *name);
    
    // Test optional types
    auto active = first_row.get<std::optional<bool>>("is_active");
    ASSERT_TRUE(active) << active.error().message;
    EXPECT_TRUE(*active);
    
    // Third row has is_active = 0
    const auto& third_row = results.at(2);
    auto third_active = third_row.get<std::optional<bool>>("is_active");
    ASSERT_TRUE(third_active) << third_active.error().message;
    EXPECT_FALSE(**third_active);
}

// Test accessing values via member pointers
TEST_F(ResultTest, AccessByMemberPtr) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    ASSERT_GT(results.size(), 0);
    
    const auto& first_row = results.at(0);
    
    // Access by member pointer
    auto id = first_row.get<&Users::id>();
    ASSERT_TRUE(id) << id.error().message;
    EXPECT_EQ(1, *id);
    
    auto name = first_row.get<&Users::name>();
    ASSERT_TRUE(name) << name.error().message;
    EXPECT_EQ("John Doe", *name);
    
    // Test optional access
    auto active = first_row.get_optional<&Users::is_active>();
    ASSERT_TRUE(active) << active.error().message;
    EXPECT_TRUE(**active);
}

// Test iteration
TEST_F(ResultTest, Iteration) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    
    // Collect all IDs by iterating
    std::vector<int> ids;
    for (const auto& row : results) {
        auto id = row.get<int>("id");
        ASSERT_TRUE(id) << id.error().message;
        ids.push_back(*id);
    }
    
    ASSERT_EQ(3, ids.size());
    EXPECT_EQ(1, ids[0]);
    EXPECT_EQ(2, ids[1]);
    EXPECT_EQ(3, ids[2]);
}

// Test transformation
TEST_F(ResultTest, Transformation) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    
    // Define a user structure to transform into
    struct UserData {
        int id;
        std::string name;
        int age;
    };
    
    // Transform rows into UserData objects
    auto users = results.transform<UserData>([](const relx::result::Row& row) -> relx::result::ResultProcessingResult<UserData> {
        auto id = row.get<int>("id");
        auto name = row.get<std::string>("name");
        auto age = row.get<int>("age");
        
        if (!id || !name || !age) {
            return std::unexpected(relx::result::ResultError{"Failed to extract user data"});
        }
        
        return UserData{*id, *name, *age};
    });
    
    ASSERT_EQ(3, users.size());
    EXPECT_EQ(1, users[0].id);
    EXPECT_EQ("John Doe", users[0].name);
    EXPECT_EQ(30, users[0].age);
}

// Test malformed data handling
TEST_F(ResultTest, MalformedData) {
    // Test empty result
    auto empty_result = relx::result::parse(query_, "");
    ASSERT_TRUE(empty_result) << empty_result.error().message;
    EXPECT_TRUE(empty_result->empty());
    
    // Test malformed result (missing column in one row)
    std::string malformed = 
        "id|name|email\n"
        "1|John Doe\n";  // Missing email column
        
    auto malformed_result = relx::result::parse(query_, malformed);
    ASSERT_TRUE(malformed_result) << malformed_result.error().message;
    ASSERT_EQ(1, malformed_result->size());
    
    // The row should still be accessible, but with fewer cells
    const auto& first_row = malformed_result->at(0);
    EXPECT_EQ(2, first_row.size());  // Only two cells were parsed
    
    // Accessing the missing cell should fail
    auto email = first_row.get<std::string>("email");
    EXPECT_FALSE(email);
}

// Test structured binding support
TEST_F(ResultTest, StructuredBinding) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    
    // Use structured binding via the 'as' helper with explicit column names
    std::vector<std::tuple<int, std::string, int>> user_data;
    
    for (const auto& [id, name, age] : results.as<int, std::string, int>(
        std::array<std::string, 3>{"id", "name", "age"})) {
        user_data.emplace_back(id, name, age);
    }
    
    ASSERT_EQ(3, user_data.size());
    
    // Check first row values
    EXPECT_EQ(1, std::get<0>(user_data[0]));
    EXPECT_EQ("John Doe", std::get<1>(user_data[0]));
    EXPECT_EQ(30, std::get<2>(user_data[0]));
    
    // Check second row values
    EXPECT_EQ(2, std::get<0>(user_data[1]));
    EXPECT_EQ("Jane Smith", std::get<1>(user_data[1]));
    EXPECT_EQ(28, std::get<2>(user_data[1]));
}

// Test structured binding with custom column indices
TEST_F(ResultTest, StructuredBindingWithCustomIndices) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    
    // Use structured binding with explicit column indices - using id(0), name(1), and is_active(4)
    std::vector<std::tuple<int, std::string, bool>> user_active_data;
    
    for (const auto& [id, name, active] : results.as<int, std::string, bool>(std::array<size_t, 3>{0, 1, 4})) {
        user_active_data.emplace_back(id, name, active);
    }
    
    ASSERT_EQ(3, user_active_data.size());
    
    // Check values to ensure we got the right columns
    EXPECT_EQ(1, std::get<0>(user_active_data[0]));
    EXPECT_EQ("John Doe", std::get<1>(user_active_data[0]));
    EXPECT_EQ(true, std::get<2>(user_active_data[0]));
    
    // Third user has is_active = false
    EXPECT_EQ(3, std::get<0>(user_active_data[2]));
    EXPECT_EQ("Bob Johnson", std::get<1>(user_active_data[2]));
    EXPECT_EQ(false, std::get<2>(user_active_data[2]));
}

// Test structured binding with column names
TEST_F(ResultTest, StructuredBindingWithColumnNames) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    
    // Use structured binding with explicit column names - age, score, email (different order)
    std::vector<std::tuple<int, double, std::string>> user_detail_data;
    
    for (const auto& [age, score, email] : results.as<int, double, std::string>(
        std::array<std::string, 3>{"age", "score", "email"})) {
        user_detail_data.emplace_back(age, score, email);
    }
    
    ASSERT_EQ(3, user_detail_data.size());
    
    // Check values to ensure we got the right columns in the right order
    EXPECT_EQ(30, std::get<0>(user_detail_data[0]));
    EXPECT_EQ(95.5, std::get<1>(user_detail_data[0]));
    EXPECT_EQ("john@example.com", std::get<2>(user_detail_data[0]));
    
    EXPECT_EQ(35, std::get<0>(user_detail_data[2]));
    EXPECT_EQ(85.7, std::get<1>(user_detail_data[2]));
    EXPECT_EQ("bob@example.com", std::get<2>(user_detail_data[2]));
}

// Test structured binding with schema
TEST_F(ResultTest, StructuredBindingWithSchema) {
    auto result = relx::result::parse(query_, raw_results_);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    
    // Use structured binding via the 'with_schema' helper with schema information
    std::vector<std::tuple<int, std::string, int>> user_data;
    
    // Option 1: Use with table type parameter and member pointers
    for (const auto& [id, name, age] : results.with_schema<Users>(&Users::id, &Users::name, &Users::age)) {
        user_data.emplace_back(id, name, age);
    }
    
    ASSERT_EQ(3, user_data.size());
    
    // Check first row values
    EXPECT_EQ(1, std::get<0>(user_data[0]));
    EXPECT_EQ("John Doe", std::get<1>(user_data[0]));
    EXPECT_EQ(30, std::get<2>(user_data[0]));
    
    // Check second row values
    EXPECT_EQ(2, std::get<0>(user_data[1]));
    EXPECT_EQ("Jane Smith", std::get<1>(user_data[1]));
    EXPECT_EQ(28, std::get<2>(user_data[1]));
    
    // Clear and test option 2: with table instance and member pointers
    user_data.clear();
    for (const auto& [id, name, age] : results.with_schema(users, &Users::id, &Users::name, &Users::age)) {
        user_data.emplace_back(id, name, age);
    }
    
    ASSERT_EQ(3, user_data.size());
    EXPECT_EQ(1, std::get<0>(user_data[0]));
    EXPECT_EQ("John Doe", std::get<1>(user_data[0]));
    EXPECT_EQ(30, std::get<2>(user_data[0]));
} 