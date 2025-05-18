#include <relx/schema.hpp>
#include <type_traits>
#include <string>
#include <concepts>
#include <iostream>

using namespace relx::schema;

// Define various test types from concept tests

// Valid column type with all required operations
struct ValidColumnType {
    static constexpr auto sql_type_name = "CUSTOM";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const ValidColumnType&) { return "test"; }
    static ValidColumnType from_sql_string(const std::string&) { return {}; }
};

// Invalid column type missing operations
struct InvalidColumnType {
    static constexpr auto sql_type_name = "INVALID";
};

// Valid table type with name
struct ValidTable {
    static constexpr auto table_name = "valid_table";
    column<ValidTable, "id", int> id;
};

// Types from basic_concepts_test
// A basic concept for numeric types
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// A concept for string-like types
template <typename T>
concept StringLike = requires(T t) {
    { t.c_str() } -> std::convertible_to<const char*>;
    { t.length() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
};

// A concept for container types
template <typename T>
concept Container = requires(T t) {
    typename T::value_type;
    typename T::iterator;
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

struct CustomString {
    const char* c_str() const { return "test"; }
    std::size_t length() const { return 4; }
    bool empty() const { return false; }
};

struct NotAString {
    int value;
};

// Test table with just columns
struct SimpleTable {
    static constexpr auto table_name = "simple_table";
    
    column<SimpleTable, "id", int> id;
    column<SimpleTable, "name", std::string> name_col;
    column<SimpleTable, "active", bool> active;
};

// Test table with nullable columns
struct TableWithNullables {
    static constexpr auto table_name = "nullable_table";
    
    column<TableWithNullables, "id", int> id;
    column<TableWithNullables, "name", std::optional<std::string>> name_col;
    column<TableWithNullables, "description", std::optional<std::string>> description;
};

// Test table with constraints
struct UsersTable {
    static constexpr auto table_name = "users";
    
    column<UsersTable, "id", int> id;
    column<UsersTable, "username", std::string> username;
    column<UsersTable, "email", std::string> email;
    
    table_primary_key<&UsersTable::id> pk;
    relx::schema::index<&UsersTable::email> email_idx{index_type::unique};
};

// Test table with foreign key constraint
struct PostsTable {
    static constexpr auto table_name = "posts";
    
    column<PostsTable, "id", int> id;
    column<PostsTable, "title", std::string> title;
    column<PostsTable, "user_id", int> user_id;
    
    table_primary_key<&PostsTable::id> pk;
    foreign_key<&PostsTable::user_id, &UsersTable::id> user_fk;
};

// All static assertions from original tests
// ColumnTypeConcept tests
static_assert(ColumnTypeConcept<int>, "int should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<double>, "double should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<std::string>, "std::string should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<bool>, "bool should satisfy ColumnTypeConcept");
static_assert(ColumnTypeConcept<ValidColumnType>, "ValidColumnType should satisfy ColumnTypeConcept");

// is_column concept tests
static_assert(is_column<column<SimpleTable, "id", int>>, "column<id, int> should satisfy is_column");
static_assert(is_column<column<SimpleTable, "name", std::optional<std::string>>>, "column<name, std::optional<std::string>> should satisfy is_column");
static_assert(!is_column<int>, "int should not satisfy is_column");
static_assert(!is_column<std::string>, "std::string should not satisfy is_column");

// is_constraint concept tests
static_assert(is_constraint<table_primary_key<&ValidTable::id>>, "primary_key should satisfy is_constraint");
static_assert(!is_constraint<column<SimpleTable, "id", int>>, "column should not satisfy is_constraint");
static_assert(!is_constraint<column<SimpleTable, "name", std::optional<std::string>>>, "column with std::optional should not satisfy is_constraint");

// TableConcept tests
static_assert(TableConcept<ValidTable>, "ValidTable should satisfy TableConcept");
static_assert(TableConcept<SimpleTable>, "SimpleTable should satisfy TableConcept");
static_assert(TableConcept<TableWithNullables>, "TableWithNullables should satisfy TableConcept");
static_assert(TableConcept<UsersTable>, "UsersTable should satisfy TableConcept");
static_assert(TableConcept<PostsTable>, "PostsTable should satisfy TableConcept");
static_assert(!TableConcept<int>, "int should not satisfy TableConcept");
static_assert(!TableConcept<std::string>, "std::string should not satisfy TableConcept");

// Basic concepts tests
static_assert(Numeric<int>, "int should satisfy Numeric");
static_assert(Numeric<double>, "double should satisfy Numeric");
static_assert(!Numeric<std::string>, "std::string should not satisfy Numeric");

static_assert(StringLike<std::string>, "std::string should satisfy StringLike");
static_assert(StringLike<CustomString>, "CustomString should satisfy StringLike");
static_assert(!StringLike<NotAString>, "NotAString should not satisfy StringLike");

static_assert(Container<std::string>, "std::string should satisfy Container");
static_assert(!Container<int>, "int should not satisfy Container");

// FixedString tests
using IdCol = column<SimpleTable, "id", int>;
static_assert(std::string_view(IdCol::name) == "id", "FixedString name should be 'id'");

using LongNameCol = column<SimpleTable, "very_long_column_name_for_testing", int>;
static_assert(std::string_view(LongNameCol::name) == "very_long_column_name_for_testing", 
             "FixedString should work with longer names");

using EmptyCol = column<SimpleTable, "", int>;
static_assert(std::string_view(EmptyCol::name) == "", "Empty FixedString should work");

int main() {
    std::println("All static assertions passed!");
    return 0;
} 