#include <sqllib/schema.hpp>
#include <type_traits>
#include <string>

using namespace sqllib::schema;

// Define various test types to check against concepts

// Valid column type with all required operations
struct ValidColumnType {
    static constexpr auto sql_type_name = "CUSTOM";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const ValidColumnType&) { return "test"; }
    static ValidColumnType from_sql_string(const std::string&) { return {}; }
};

// Valid table type with name
struct ValidTable {
    static constexpr auto name = std::string_view("valid_table");
    column<"id", int> id;
};

// Test table with just columns
struct SimpleTable {
    static constexpr auto name = std::string_view("simple_table");
    
    column<"id", int> id;
    column<"name", std::string> name_col;
    column<"active", bool> active;
};

// Test table with nullable columns
struct TableWithNullables {
    static constexpr auto name = std::string_view("nullable_table");
    
    column<"id", int> id;
    column<"name", std::optional<std::string>> name_col;
    column<"description", std::optional<std::string>> description;
};

// Test table with constraints
struct UsersTable {
    static constexpr auto name = std::string_view("users");
    
    column<"id", int> id;
    column<"username", std::string> username;
    column<"email", std::string> email;
    
    primary_key<&UsersTable::id> pk;
    sqllib::schema::index<&UsersTable::email> email_idx{index_type::unique};
};

// Static assertions for concept tests
// ColumnTypeConcept tests
static_assert(ColumnTypeConcept<int>, "int should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<double>, "double should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<std::string>, "std::string should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<bool>, "bool should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<ValidColumnType>, "ValidColumnType should satisfy ColumnTypeConcept");

// is_column concept tests
static_assert(is_column<column<"id", int>>, "column<id, int> should satisfy is_column");
static_assert(is_column<column<"name", std::optional<std::string>>>, "column<name, std::optional<std::string>> should satisfy is_column");
static_assert(!is_column<int>, "int should not satisfy is_column");
static_assert(!is_column<std::string>, "std::string should not satisfy is_column");

// is_constraint concept tests
static_assert(is_constraint<primary_key<&ValidTable::id>>, "primary_key should satisfy is_constraint");

// TableConcept tests
static_assert(TableConcept<ValidTable>, "ValidTable should satisfy TableConcept");
static_assert(TableConcept<SimpleTable>, "SimpleTable should satisfy TableConcept");
static_assert(TableConcept<TableWithNullables>, "TableWithNullables should satisfy TableConcept");
static_assert(TableConcept<UsersTable>, "UsersTable should satisfy TableConcept");
static_assert(!TableConcept<int>, "int should not satisfy TableConcept");
static_assert(!TableConcept<std::string>, "std::string should not satisfy TableConcept");

// Custom type that satisfies the column concept
template <FixedString Name, ColumnTypeConcept T>
struct custom_column {
    using value_type = T;
    static constexpr auto name = Name;
    static constexpr auto sql_type = column_traits<T>::sql_type_name;
    static constexpr bool nullable = false;
    
    std::string sql_definition() const {
        return std::string(std::string_view(name)) + " " + 
               std::string(std::string_view(sql_type)) + " CUSTOM";
    }
};

// Simple main function for testing
int main() {
    // If the static_asserts compile, we're good
    custom_column<"test", int> test_col;
    return 0;
} 