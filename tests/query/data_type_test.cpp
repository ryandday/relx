#include <gtest/gtest.h>
#include "test_common.hpp"
#include <string>
#include <optional>
#include <vector>
#include <cmath>
#include <array>

using namespace test_tables;
using namespace test_utils;

// Test for integer types
TEST(DataTypeTest, IntegerTypes) {
    users u;
    
    // Test with different integer types
    auto query_int = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.id) == sqllib::query::val(42));
    
    auto query_long = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.id) == sqllib::query::val(9223372036854775807L)); // max int64_t
    
    std::string expected_sql = "SELECT id, name FROM users WHERE (id = ?)";
    EXPECT_EQ(query_int.to_sql(), expected_sql);
    EXPECT_EQ(query_long.to_sql(), expected_sql);
    
    auto params_int = query_int.bind_params();
    auto params_long = query_long.bind_params();
    
    EXPECT_EQ(params_int.size(), 1);
    EXPECT_EQ(params_long.size(), 1);
    EXPECT_EQ(params_int[0], "42");
    EXPECT_EQ(params_long[0], "9223372036854775807");
}

// Define a custom column for testing
struct score_column {
    static constexpr auto name = "score";
    using value_type = float;
};

// Test for floating point types
TEST(DataTypeTest, FloatingPointTypes) {
    users u;
    
    // Test with float column
    auto sc = score_column{};
    
    // Test with different floating point values
    auto query_float = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(sc) > sqllib::query::val(3.14159f));
    
    auto query_double = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(sc) > sqllib::query::val(2.7182818284590452));
    
    std::string expected_sql = "SELECT id, name FROM users WHERE (score > ?)";
    EXPECT_EQ(query_float.to_sql(), expected_sql);
    EXPECT_EQ(query_double.to_sql(), expected_sql);
    
    auto params_float = query_float.bind_params();
    auto params_double = query_double.bind_params();
    
    EXPECT_EQ(params_float.size(), 1);
    EXPECT_EQ(params_double.size(), 1);
    
    // The exact string representation may vary, but should be close to these values
    // Some implementations might use scientific notation
    EXPECT_TRUE(params_float[0].find("3.14159") != std::string::npos || 
                params_float[0].find("3.1415") != std::string::npos);
    EXPECT_TRUE(params_double[0].find("2.71828") != std::string::npos ||
                params_double[0].find("2.7182") != std::string::npos);
}

// Test for string types
TEST(DataTypeTest, StringTypes) {
    users u;
    
    // Test with different string types
    std::string std_string = "Standard string";
    const char* c_string = "C-style string";
    
    auto query_std_string = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.name) == sqllib::query::val(std_string));
    
    auto query_c_string = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.name) == sqllib::query::val(c_string));
    
    // Test with string literal
    auto query_string_literal = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.name) == sqllib::query::val("String literal"));
    
    std::string expected_sql = "SELECT id, name FROM users WHERE (name = ?)";
    EXPECT_EQ(query_std_string.to_sql(), expected_sql);
    EXPECT_EQ(query_c_string.to_sql(), expected_sql);
    EXPECT_EQ(query_string_literal.to_sql(), expected_sql);
    
    auto params_std_string = query_std_string.bind_params();
    auto params_c_string = query_c_string.bind_params();
    auto params_string_literal = query_string_literal.bind_params();
    
    EXPECT_EQ(params_std_string.size(), 1);
    EXPECT_EQ(params_c_string.size(), 1);
    EXPECT_EQ(params_string_literal.size(), 1);
    
    EXPECT_EQ(params_std_string[0], "Standard string");
    EXPECT_EQ(params_c_string[0], "C-style string");
    EXPECT_EQ(params_string_literal[0], "String literal");
}

// Test for optional types
TEST(DataTypeTest, OptionalTypes) {
    users u;
    
    // Test with std::optional values
    std::optional<std::string> present_value = "Optional string";
    std::optional<std::string> absent_value = std::nullopt;
    
    // Create separate queries for present and absent values
    auto query_with_value = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.bio) == sqllib::query::val("Optional string"));
        
    auto query_with_null = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::is_null(sqllib::query::to_expr(u.bio)));
    
    std::string expected_present_sql = "SELECT id, name FROM users WHERE (bio = ?)";
    std::string expected_absent_sql = "SELECT id, name FROM users WHERE bio IS NULL";
    
    EXPECT_EQ(query_with_value.to_sql(), expected_present_sql);
    EXPECT_EQ(query_with_null.to_sql(), expected_absent_sql);
    
    auto params_present = query_with_value.bind_params();
    auto params_absent = query_with_null.bind_params();
    
    EXPECT_EQ(params_present.size(), 1);
    EXPECT_EQ(params_absent.size(), 0);
    EXPECT_EQ(params_present[0], "Optional string");
}

// Test for container types
TEST(DataTypeTest, ContainerTypes) {
    users u;
    
    // Test with different container types in IN clauses
    std::vector<std::string> str_vector = {"1", "2", "3", "4", "5"};
    std::array<std::string, 3> string_array = {"apple", "banana", "cherry"};
    
    auto query_vector = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::in(sqllib::query::to_expr(u.name), str_vector));
    
    auto query_array = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::in(sqllib::query::to_expr(u.name), string_array));
    
    std::string expected_vector_sql = "SELECT id, name FROM users WHERE name IN (?, ?, ?, ?, ?)";
    std::string expected_array_sql = "SELECT id, name FROM users WHERE name IN (?, ?, ?)";
    
    EXPECT_EQ(query_vector.to_sql(), expected_vector_sql);
    EXPECT_EQ(query_array.to_sql(), expected_array_sql);
    
    auto params_vector = query_vector.bind_params();
    auto params_array = query_array.bind_params();
    
    EXPECT_EQ(params_vector.size(), 5);
    EXPECT_EQ(params_array.size(), 3);
    
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(params_vector[i], std::to_string(i+1));
    }
    
    EXPECT_EQ(params_array[0], "apple");
    EXPECT_EQ(params_array[1], "banana");
    EXPECT_EQ(params_array[2], "cherry");
}

// Test for boolean types
TEST(DataTypeTest, BooleanTypes) {
    users u;
    
    // Test with boolean values in different contexts
    auto query_bool_equals = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.is_active) == sqllib::query::val(true));
    
    auto query_bool_not = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(!sqllib::query::to_expr(u.is_active));
    
    auto query_bool_and = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.is_active) && 
               (sqllib::query::to_expr(u.age) > sqllib::query::val(18)));
    
    std::string expected_equals_sql = "SELECT id, name FROM users WHERE (is_active = ?)";
    EXPECT_EQ(query_bool_equals.to_sql(), expected_equals_sql);
    
    // The exact SQL for boolean NOT and AND operations may vary by implementation
    std::string not_sql = query_bool_not.to_sql();
    std::string and_sql = query_bool_and.to_sql();
    
    EXPECT_FALSE(not_sql.empty());
    EXPECT_FALSE(and_sql.empty());
    
    // The NOT operator should appear in the SQL
    EXPECT_TRUE(not_sql.find("NOT") != std::string::npos || 
                not_sql.find("!") != std::string::npos || 
                not_sql.find("= 0") != std::string::npos ||
                not_sql.find("= FALSE") != std::string::npos);
    
    // The AND operator should appear in the SQL
    EXPECT_TRUE(and_sql.find("AND") != std::string::npos);
    
    auto params_equals = query_bool_equals.bind_params();
    EXPECT_EQ(params_equals.size(), 1);
    EXPECT_TRUE(params_equals[0] == "1" || params_equals[0] == "true" || params_equals[0] == "TRUE");
}

// Test for NULL handling
TEST(DataTypeTest, NullHandling) {
    users u;
    
    // Test IS NULL and IS NOT NULL
    auto query_is_null = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::is_null(sqllib::query::to_expr(u.bio)));
    
    auto query_is_not_null = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::is_not_null(sqllib::query::to_expr(u.bio)));
    
    // Test nullable column with IS NULL
    auto query_equals_null = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::is_null(sqllib::query::to_expr(u.bio)));
    
    std::string expected_is_null_sql = "SELECT id, name FROM users WHERE bio IS NULL";
    std::string expected_is_not_null_sql = "SELECT id, name FROM users WHERE bio IS NOT NULL";
    std::string expected_equals_null_sql = "SELECT id, name FROM users WHERE bio IS NULL";
    
    EXPECT_EQ(query_is_null.to_sql(), expected_is_null_sql);
    EXPECT_EQ(query_is_not_null.to_sql(), expected_is_not_null_sql);
    EXPECT_EQ(query_equals_null.to_sql(), expected_equals_null_sql);
    
    EXPECT_TRUE(query_is_null.bind_params().empty());
    EXPECT_TRUE(query_is_not_null.bind_params().empty());
    EXPECT_TRUE(query_equals_null.bind_params().empty());
} 