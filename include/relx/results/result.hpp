#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <optional>
#include <variant>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <memory>
#include <utility>
#include <ranges>
#include <map>
#include <expected>
#include <concepts>
#include <array>

#include "../schema/core.hpp"
#include "../schema/column.hpp"
#include "../schema/table.hpp"
#include "../query/core.hpp"

namespace relx {
namespace result {

// Helper type to extract class type from member pointer
template <typename T>
struct class_of_t;

template <typename Class, typename T>
struct class_of_t<T Class::*> { 
    using type = Class; 
};

template <typename T>
using class_of_t_t = typename class_of_t<T>::type;

/// @brief Error type for result processing operations
struct ResultError {
    std::string message;
};

/// @brief Type alias for result of processing operations
template <typename T>
using ResultProcessingResult = std::expected<T, ResultError>;

/// @brief Helper to get the class type from a member pointer
template <typename T, typename C>
static constexpr auto class_of(T C::*) {
    return (C*)nullptr;
}

/// @brief Gets the column name from a column member pointer
template <auto MemberPtr>
constexpr std::string_view get_column_name() {
    using Class = class_of_t_t<decltype(MemberPtr)>;
    using ColumnType = std::remove_reference_t<decltype(std::declval<Class>().*MemberPtr)>;
    return ColumnType::column_name;
}

/// @brief Helper to get the value type from a column member pointer
template <typename Table, typename ColumnMemberPtr>
struct column_member_value {
    using column_type = std::remove_reference_t<decltype(std::declval<Table>().*std::declval<ColumnMemberPtr>())>;
    using type = typename column_type::value_type;
};

template <typename Table, typename ColumnMemberPtr>
using column_member_value_t = typename column_member_value<Table, ColumnMemberPtr>::type;

/// @brief Get column name from a member pointer
template <typename Table, typename ColumnMemberPtr>
std::string get_column_name_from_ptr(ColumnMemberPtr ptr) {
    using ColumnType = std::remove_reference_t<decltype(std::declval<Table>().*ptr)>;
    return std::string(ColumnType::name);
}

/// @brief Represents a single cell value from a database result
class Cell {
public:
    /// @brief Constructs a cell with a raw string value from the database
    explicit Cell(std::string value) : value_(std::move(value)) {}

    /// @brief Check if the cell contains a NULL value
    bool is_null() const {
        return value_ == "NULL";
    }

    /// @brief Get the raw string value
    const std::string& raw_value() const {
        return value_;
    }

    /// @brief Parse the cell's value as the specified type
    /// @tparam T The target C++ type
    /// @return The parsed value
    template <typename T>
    ResultProcessingResult<T> as() const {
        return as<T>(false);
    }
    
    /// @brief Parse the cell's value as the specified type with an option to allow numeric boolean conversion
    /// @tparam T The target C++ type
    /// @param allow_numeric_bools Whether to allow integers (0/1) to be converted to booleans
    /// @return The parsed value
    template <typename T>
    ResultProcessingResult<T> as(bool allow_numeric_bools) const {
        if (is_null()) {
            if constexpr (is_optional_v<T>) {
                return T{std::nullopt};
            } else {
                return std::unexpected(ResultError{"Cannot convert NULL to non-optional type"});
            }
        }
        
        // More strict type checking
        if constexpr (std::is_same_v<T, bool>) {
            const auto lower = to_lower(value_);
            
            // Always accept explicit boolean strings
            if (lower == "true") {
                return true;
            } else if (lower == "false") {
                return false;
            } else if (lower == "t") {  // PostgreSQL format
                return true;
            } else if (lower == "f") {  // PostgreSQL format
                return false;
            } else if (allow_numeric_bools) {
                // Only allow numeric conversion if explicitly allowed
                if (lower == "1") {
                    return true;
                } else if (lower == "0") {
                    return false;
                }
            }
            
            return std::unexpected(ResultError{
                std::string("Cannot convert '") + value_ + "' to boolean: not a boolean value"
            });
        } else if constexpr (std::is_same_v<T, int> || 
                             std::is_same_v<T, long> || 
                             std::is_same_v<T, long long>) {
            // For integer types, reject any attempt to convert from boolean strings
            const auto lower = to_lower(value_);
            if (lower == "true" || lower == "false") {
                return std::unexpected(ResultError{"Cannot convert boolean value to integer type"});
            }
            
            // Use the appropriate parsing for the integer type
            try {
                if (!is_valid_integer(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to integer: invalid format"});
                }
                
                if constexpr (std::is_same_v<T, int>) {
                    return std::stoi(value_);
                } else if constexpr (std::is_same_v<T, long>) {
                    return std::stol(value_);
                } else if constexpr (std::is_same_v<T, long long>) {
                    return std::stoll(value_);
                }
            } catch (const std::exception& e) {
                return std::unexpected(ResultError{
                    std::string("Error parsing cell value '") + value_ + "' to integer: " + e.what()
                });
            }
        } else if constexpr (schema::ColumnTypeConcept<T>) {
            try {
                return schema::column_traits<T>::from_sql_string(value_);
            } catch (const std::exception& e) {
                return std::unexpected(ResultError{
                    std::string("Error parsing cell value '") + value_ + "' to column type: " + e.what()
                });
            }
        } else {
            // Use appropriate parsing based on the target type
            return parse_value<T>(allow_numeric_bools);
        }
    }

private:
    std::string value_;
    
    // Helper to detect optional types
    template <typename T>
    static constexpr bool is_optional_v = false;
    
    template <typename T>
    static constexpr bool is_optional_v<std::optional<T>> = true;
    
    // String helpers
    static std::string to_lower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return str;
    }
    
    // Check if a value is likely a boolean
    bool is_boolean_value(const std::string& str) const {
        // Only accept "true", "false", "0", or "1" as possible boolean values
        return str == "true" || str == "false" || str == "0" || str == "1";
    }
    
    // Type-specific parsing implementations
    template <typename T>
    ResultProcessingResult<T> parse_value(bool allow_numeric_bools) const {
        try {
            if constexpr (std::is_same_v<T, int>) {
                // Validate the string contains only digits and optional sign
                if (!is_valid_integer(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to int: invalid format"});
                }
                return std::stoi(value_);
            } else if constexpr (std::is_same_v<T, long>) {
                if (!is_valid_integer(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to long: invalid format"});
                }
                return std::stol(value_);
            } else if constexpr (std::is_same_v<T, long long>) {
                if (!is_valid_integer(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to long long: invalid format"});
                }
                return std::stoll(value_);
            } else if constexpr (std::is_same_v<T, unsigned long>) {
                if (!is_valid_unsigned_integer(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to unsigned long: invalid format"});
                }
                return std::stoul(value_);
            } else if constexpr (std::is_same_v<T, unsigned long long>) {
                if (!is_valid_unsigned_integer(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to unsigned long long: invalid format"});
                }
                return std::stoull(value_);
            } else if constexpr (std::is_same_v<T, float>) {
                if (!is_valid_float(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to float: invalid format"});
                }
                return std::stof(value_);
            } else if constexpr (std::is_same_v<T, double>) {
                if (!is_valid_float(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to double: invalid format"});
                }
                return std::stod(value_);
            } else if constexpr (std::is_same_v<T, long double>) {
                if (!is_valid_float(value_)) {
                    return std::unexpected(ResultError{"Cannot convert to long double: invalid format"});
                }
                return std::stold(value_);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return value_;
            } else if constexpr (is_optional_v<T>) {
                using ValueType = typename T::value_type;
                auto result = as<ValueType>(allow_numeric_bools);
                if (result) {
                    return T{*result};
                } else {
                    return T{std::nullopt};
                }
            } else {
                // For any other type, throw a clear conversion error
                return std::unexpected(ResultError{
                    std::string("Unsupported type conversion from '") + value_ + "'"
                });
            }
        } catch (const std::exception& e) {
            return std::unexpected(ResultError{
                std::string("Error parsing cell value '") + value_ + "': " + e.what()
            });
        }
    }
    
    // Helper functions for validation
    bool is_valid_integer(const std::string& str) const {
        if (str.empty()) return false;
        
        size_t start = 0;
        if (str[0] == '-' || str[0] == '+') start = 1;
        
        return str.length() > start && 
               std::all_of(str.begin() + start, str.end(), 
                          [](char c) { return std::isdigit(c); });
    }
    
    bool is_valid_unsigned_integer(const std::string& str) const {
        if (str.empty()) return false;
        
        size_t start = 0;
        if (str[0] == '+') start = 1;
        
        return str.length() > start && 
               std::all_of(str.begin() + start, str.end(), 
                          [](char c) { return std::isdigit(c); });
    }
    
    bool is_valid_float(const std::string& str) const {
        if (str.empty()) return false;
        
        bool has_digit = false;
        bool has_decimal = false;
        bool has_exponent = false;
        
        size_t i = 0;
        if (str[0] == '-' || str[0] == '+') i++;
        
        for (; i < str.length(); i++) {
            char c = str[i];
            
            if (std::isdigit(c)) {
                has_digit = true;
            } else if (c == '.') {
                if (has_decimal || has_exponent) return false;
                has_decimal = true;
            } else if (c == 'e' || c == 'E') {
                if (!has_digit || has_exponent) return false;
                has_exponent = true;
                
                if (i + 1 < str.length() && (str[i + 1] == '+' || str[i + 1] == '-')) i++;
                if (i + 1 >= str.length()) return false; // No digits after exponent
            } else {
                return false;
            }
        }
        
        return has_digit;
    }
    
    // Specialization for optional types
    template <typename T>
    ResultProcessingResult<std::optional<T>> parse_value(bool allow_numeric_bools) const requires requires { parse_value<T>(allow_numeric_bools); } {
        if (is_null()) {
            return std::optional<T>{std::nullopt};
        }
        
        auto result = parse_value<T>(allow_numeric_bools);
        if (!result) {
            return std::unexpected(result.error());
        }
        return std::optional<T>{*result};
    }
};

/// @brief Represents a single row from a database result
class Row {
public:
    /// @brief Constructs a row with cells and column names
    /// @param cells The raw cell values
    /// @param column_names The column names (optional)
    explicit Row(std::vector<Cell> cells, 
                std::vector<std::string> column_names = {})
        : cells_(std::move(cells)), 
          column_names_(std::move(column_names)) {}
    
    /// @brief Get a cell by index
    /// @param index The zero-based index of the cell
    /// @return The cell at the specified index
    ResultProcessingResult<const Cell*> get_cell(size_t index) const {
        if (index >= cells_.size()) {
            return std::unexpected(ResultError{"Cell index out of range"});
        }
        return &cells_[index];
    }
    
    /// @brief Get a cell by column name
    /// @param name The name of the column
    /// @return The cell for the specified column
    ResultProcessingResult<const Cell*> get_cell(const std::string& name) const {
        if (column_names_.empty()) {
            return std::unexpected(ResultError{"Column names not available"});
        }
        
        for (size_t i = 0; i < column_names_.size(); ++i) {
            if (column_names_[i] == name && i < cells_.size()) {
                return &cells_[i];
            }
            else if (column_names_[i] == name) {
                // Found the column name but missing the corresponding cell
                return std::unexpected(ResultError{"Column found but missing cell data: " + name});
            }
        }
        
        return std::unexpected(ResultError{"Column name not found: " + name});
    }
    
    /// @brief Get a typed value by index
    /// @tparam T The target C++ type
    /// @param index The zero-based index of the cell
    /// @return The parsed value
    template <typename T>
    ResultProcessingResult<T> get(size_t index) const {
        return get<T>(index, false);
    }
    
    /// @brief Get a typed value by index with allow_numeric_bools
    /// @tparam T The target C++ type
    /// @param index The zero-based index of the cell
    /// @param allow_numeric_bools Whether to allow integers (0/1) to be converted to booleans
    /// @return The parsed value
    template <typename T>
    ResultProcessingResult<T> get(size_t index, bool allow_numeric_bools) const {
        auto cell = get_cell(index);
        if (!cell) {
            return std::unexpected(cell.error());
        }
        return (*cell)->template as<T>(allow_numeric_bools);
    }
    
    /// @brief Get a typed value by column name
    /// @tparam T The target C++ type
    /// @param name The name of the column
    /// @return The parsed value
    template <typename T>
    ResultProcessingResult<T> get(const std::string& name) const {
        return get<T>(name, false);
    }
    
    /// @brief Get a typed value by column name with allow_numeric_bools
    /// @tparam T The target C++ type
    /// @param name The name of the column
    /// @param allow_numeric_bools Whether to allow integers (0/1) to be converted to booleans
    /// @return The parsed value
    template <typename T>
    ResultProcessingResult<T> get(const std::string& name, bool allow_numeric_bools) const {
        auto cell = get_cell(name);
        if (!cell) {
            return std::unexpected(cell.error());
        }
        return (*cell)->template as<T>(allow_numeric_bools);
    }
    
    /// @brief Get a typed value using a schema column object
    /// @tparam T The target C++ type
    /// @tparam ColType The column type
    /// @param column The column object
    /// @return The parsed value
    template <typename T, typename ColType>
    ResultProcessingResult<T> get(const ColType& column) const {
        return get<T>(column, false);
    }
    
    /// @brief Get a typed value using a schema column object with allow_numeric_bools
    /// @tparam T The target C++ type
    /// @tparam ColType The column type
    /// @param column The column object
    /// @param allow_numeric_bools Whether to allow integers (0/1) to be converted to booleans
    /// @return The parsed value
    template <typename T, typename ColType>
    ResultProcessingResult<T> get(const ColType& column, bool allow_numeric_bools) const {
        if constexpr (requires { { column.column_name } -> std::convertible_to<std::string_view>; }) {
            // If we have a column object with a column_name, use that
            return get<T>(std::string(column.column_name), allow_numeric_bools);
        } else if constexpr (requires { { column.name } -> std::convertible_to<std::string_view>; }) {
            // Some column-like objects might use 'name' instead
            return get<T>(std::string(column.name), allow_numeric_bools);
        } else if constexpr (std::is_same_v<ColType, std::string> || std::is_same_v<ColType, std::string_view> ||
                          std::is_convertible_v<ColType, std::string_view>) {
            // If we have a string, string_view, or convertible to string_view (like char array), use that directly
            return get<T>(std::string(column), allow_numeric_bools);
        } else if constexpr (std::is_convertible_v<ColType, size_t>) {
            // If we can convert to size_t, use as an index
            return get<T>(static_cast<size_t>(column), allow_numeric_bools);
        } else {
            // For other custom column types
            static_assert(
                requires { { column } -> std::convertible_to<std::string_view>; } ||
                requires { { column } -> std::convertible_to<size_t>; } ||
                requires { { column.name } -> std::convertible_to<std::string_view>; } ||
                requires { { column.column_name } -> std::convertible_to<std::string_view>; },
                "Column must be convertible to a string, size_t, or have a name/column_name member"
            );
            
            // This should never be reached due to the static_assert above
            return std::unexpected(ResultError{"Invalid column type"});
        }
    }
    
    /// @brief Get a typed value using a member pointer
    /// @tparam MemberPtr Pointer to a class member
    /// @return The parsed value
    template <auto MemberPtr>
    auto get() const {
        using Class = class_of_t_t<decltype(MemberPtr)>;
        using ColumnType = std::remove_reference_t<decltype(std::declval<Class>().*MemberPtr)>;
        using ValueType = typename ColumnType::value_type;
        
        // Find the column name
        static constexpr auto column_name = ColumnType::name;
        
        // Delegate to the string-based version
        return get<ValueType>(std::string(column_name));
    }
    
    /// @brief Get an optional typed value using a member pointer
    /// @tparam MemberPtr Pointer to a class member
    /// @return The parsed value as an optional
    template <auto MemberPtr>
    auto get_optional() const {
        using Class = class_of_t_t<decltype(MemberPtr)>;
        using ColumnType = std::remove_reference_t<decltype(std::declval<Class>().*MemberPtr)>;
        using ValueType = typename ColumnType::value_type;
        
        // Find the column name
        static constexpr auto column_name = ColumnType::name;
        
        // Delegate to the string-based version
        return get<std::optional<ValueType>>(std::string(column_name));
    }
    
    /// @brief Get the number of cells in this row
    /// @return The number of cells
    size_t size() const {
        return cells_.size();
    }
    
    /// @brief Get the column names
    /// @return The column names (may be empty)
    const std::vector<std::string>& column_names() const {
        return column_names_;
    }

private:
    std::vector<Cell> cells_;
    std::vector<std::string> column_names_;
};

/// @brief Class to support structured binding for ResultSet
template <typename... Types>
struct RowAdapter {
    // Store the actual tuple of values
    std::tuple<Types...> values;
    
    // Allow direct access to elements for destructuring
    template <size_t I>
    friend const auto& get(const RowAdapter& a) {
        return std::get<I>(a.values);
    }
};

/// @brief Iterator that yields RowAdapters
template <typename ResultSet, typename... Types>
struct RowIterator {
    const ResultSet& results;
    size_t index;
    std::array<size_t, sizeof...(Types)> column_indices;
    
    bool operator!=(const RowIterator& other) const {
        return index != other.index;
    }
    
    RowIterator& operator++() {
        ++index;
        return *this;
    }
    
    auto operator*() const {
        return get_row_values(std::make_index_sequence<sizeof...(Types)>{});
    }
    
private:
    template <size_t... Indices>
    RowAdapter<Types...> get_row_values(std::index_sequence<Indices...>) const {
        const auto& row = results.at(index);
        
        return RowAdapter<Types...>{
            std::tuple<Types...>{
                [&row, this]() -> std::tuple_element_t<Indices, std::tuple<Types...>> {
                    using ResultType = std::tuple_element_t<Indices, std::tuple<Types...>>;
                    // Allow numeric boolean conversion for structured binding
                    bool allow_numeric_bools = std::is_same_v<ResultType, bool>;
                    auto result = row.template get<ResultType>(column_indices[Indices], allow_numeric_bools);
                    if (result) {
                        return *result;
                    } else {
                        // Return default value for the type if conversion fails
                        return ResultType{};
                    }
                }()...
            }
        };
    }
};

/// @brief View class for structured binding support
template <typename ResultSet, typename... Types>
struct RowsView {
    const ResultSet& results;
    std::array<size_t, sizeof...(Types)> column_indices;
    
    RowIterator<ResultSet, Types...> begin() const { 
        return {results, 0, column_indices}; 
    }
    
    RowIterator<ResultSet, Types...> end() const { 
        return {results, results.size(), column_indices}; 
    }
};

/// @brief Represents the result set from a database query
class ResultSet {
public:
    /// @brief Default constructor
    ResultSet() = default;
    
    /// @brief Constructs a result set with rows and column names
    ResultSet(std::vector<Row> rows, std::vector<std::string> column_names = {})
        : rows_(std::move(rows)), column_names_(std::move(column_names)) {}

    /// @brief Get the number of rows in the result set
    /// @return The number of rows
    size_t size() const {
        return rows_.size();
    }
    
    /// @brief Check if the result set is empty
    /// @return True if there are no rows
    bool empty() const {
        return rows_.empty();
    }
    
    /// @brief Get a row by index
    /// @param index The zero-based index of the row
    /// @return The row at the specified index
    const Row& at(size_t index) const {
        return rows_.at(index);
    }
    
    /// @brief Access a row by index using the subscript operator
    /// @param index The zero-based index of the row
    /// @return The row at the specified index
    const Row& operator[](size_t index) const {
        return rows_.at(index);
    }
    
    /// @brief Get the number of columns in the result set
    /// @return The number of columns
    size_t column_count() const {
        return column_names_.size();
    }
    
    /// @brief Get the name of a column by index
    /// @param index The zero-based index of the column
    /// @return The name of the column
    std::string column_name(size_t index) const {
        if (index >= column_names_.size()) {
            throw std::out_of_range("Column index out of range");
        }
        return column_names_.at(index);
    }
    
    /// @brief Get the column names
    /// @return The column names
    const std::vector<std::string>& column_names() const {
        return column_names_;
    }
    
    /// @brief Get an iterator to the beginning of the rows
    /// @return Iterator to the first row
    auto begin() const {
        return rows_.begin();
    }
    
    /// @brief Get an iterator to the end of the rows
    /// @return Iterator past the last row
    auto end() const {
        return rows_.end();
    }
    
    /// @brief Transform each row into an object of type T
    /// @tparam T The target object type
    /// @param mapper Function that maps a Row to an object of type T
    /// @return A vector of transformed objects
    template <typename T>
    std::vector<T> transform(std::function<ResultProcessingResult<T>(const Row&)> mapper) const {
        std::vector<T> result;
        result.reserve(rows_.size());
        
        for (const auto& row : rows_) {
            auto transformed = mapper(row);
            if (transformed) {
                result.push_back(std::move(*transformed));
            }
        }
        
        return result;
    }
    
    /// @brief Helper for structured binding with explicit types
    /// @tparam Types The C++ types for each column
    /// @return A helper object that supports structured binding
    template <typename... Types>
    auto as() const {
        // Default to first N columns where N is the number of Types
        std::array<size_t, sizeof...(Types)> indices;
        for (size_t i = 0; i < sizeof...(Types); ++i) {
            indices[i] = i;
        }
        return as<Types...>(indices);
    }
    
    /// @brief Helper for structured binding with explicit types and column indices
    /// @tparam Types The C++ types for each column
    /// @param indices Array of column indices to use for structured binding
    /// @return A helper object that supports structured binding
    template <typename... Types>
    auto as(const std::array<size_t, sizeof...(Types)>& indices) const {
        return RowsView<ResultSet, Types...>{*this, indices};
    }
    
    /// @brief Helper for structured binding with explicit types and column names
    /// @tparam Types The C++ types for each column
    /// @param column_names Array of column names to use for structured binding
    /// @return A helper object that supports structured binding
    template <typename... Types>
    auto as(const std::array<std::string, sizeof...(Types)>& column_names) const {
        std::array<size_t, sizeof...(Types)> indices;
        
        for (size_t i = 0; i < sizeof...(Types); ++i) {
            bool found = false;
            for (size_t col_idx = 0; col_idx < column_names_.size(); ++col_idx) {
                if (column_names_[col_idx] == column_names[i]) {
                    indices[i] = col_idx;
                    found = true;
                    break;
                }
            }
            if (!found) {
                // If column name not found, default to index or 0
                indices[i] = (i < column_names_.size()) ? i : 0;
            }
        }
        
        return as<Types...>(indices);
    }

    /**
     * @brief Creates a structured binding view using table schema information
     * 
     * This method enables structured binding with the table's schema, eliminating
     * the need to manually specify column names or indices.
     * 
     * @tparam Table The table schema type (must satisfy TableType concept)
     * @tparam P1, P2, ... Member pointer types to column members
     * @param mp1, mp2, ... Column member pointers to bind to
     * @return A view that supports structured binding iteration
     * 
     * Example:
     * ```cpp
     * Users users;
     * for (const auto& [id, name, email] : results.with_schema<Users>(&Users::id, &Users::name, &Users::email)) {
     *    // Use the values with proper types
     * }
     * ```
     */
    template <typename Table, typename P1, typename... Ps>
    requires schema::TableConcept<Table>
    auto with_schema(P1 mp1, Ps... mps) const {
        constexpr size_t num_cols = 1 + sizeof...(Ps);
        std::array<std::string, num_cols> column_names;
        
        // Fill the column names array using our helper
        column_names[0] = get_column_name_from_ptr<Table>(mp1);
        if constexpr (sizeof...(Ps) > 0) {
            size_t i = 1;
            ((column_names[i++] = get_column_name_from_ptr<Table>(mps)), ...);
        }
        
        // Call the as<> method with the right types
        return as<column_member_value_t<Table, P1>, column_member_value_t<Table, Ps>...>(column_names);
    }

    /**
     * @brief Creates a structured binding view using table schema information
     * 
     * This overload enables structured binding with a complete table schema, 
     * making it easier to use with a pre-existing table instance.
     * 
     * @tparam Table The table schema type
     * @tparam P1, P2, ... Member pointer types to column members
     * @param table The table instance to extract schema from
     * @param mp1, mp2, ... Column member pointers to bind to
     * @return A view that supports structured binding iteration
     * 
     * Example:
     * ```cpp
     * Users users;
     * for (const auto& [id, name, email] : results.with_schema(users, &Users::id, &Users::name, &Users::email)) {
     *    // Use the values with proper types
     * }
     * ```
     */
    template <typename Table, typename P1, typename... Ps>
    requires schema::TableConcept<std::remove_reference_t<Table>>
    auto with_schema(const Table& table, P1 mp1, Ps... mps) const {
        return with_schema<std::remove_reference_t<Table>>(mp1, mps...);
    }

private:
    std::vector<Row> rows_;
    std::vector<std::string> column_names_;
};

/// @brief Parse raw results from a database into a typed ResultSet
/// @tparam Query The query type
/// @param query The query that was executed
/// @param raw_results The raw results as CSV or similar format
/// @return A ResultSet with typed rows
template <query::SqlExpr Query>
ResultProcessingResult<ResultSet> parse(const Query& query, const std::string& raw_results) {
    try {
        // Split the raw results into lines
        std::vector<std::string> lines;
        size_t pos = 0;
        size_t next_pos = 0;
        
        // Parse header line and data lines
        while ((next_pos = raw_results.find('\n', pos)) != std::string::npos) {
            lines.push_back(raw_results.substr(pos, next_pos - pos));
            pos = next_pos + 1;
        }
        
        // Add the last line if it's not empty
        if (pos < raw_results.size()) {
            lines.push_back(raw_results.substr(pos));
        }
        
        if (lines.empty()) {
            return ResultSet{};
        }
        
        // Parse column names from the first line
        std::vector<std::string> column_names;
        std::string header = lines[0];
        pos = 0;
        
        while ((next_pos = header.find('|', pos)) != std::string::npos) {
            column_names.push_back(header.substr(pos, next_pos - pos));
            pos = next_pos + 1;
        }
        
        // Add the last column
        if (pos < header.size()) {
            column_names.push_back(header.substr(pos));
        }
        
        // Parse data rows
        std::vector<Row> rows;
        
        for (size_t i = 1; i < lines.size(); ++i) {
            const auto& line = lines[i];
            
            // Skip empty lines
            if (line.empty()) {
                continue;
            }
            
            // Parse cells
            std::vector<Cell> cells;
            pos = 0;
            
            while ((next_pos = line.find('|', pos)) != std::string::npos) {
                cells.emplace_back(line.substr(pos, next_pos - pos));
                pos = next_pos + 1;
            }
            
            // Add the last cell
            if (pos < line.size()) {
                cells.emplace_back(line.substr(pos));
            }
            
            // Create a row with the cells and column names
            rows.emplace_back(std::move(cells), column_names);
        }
        
        return ResultSet{std::move(rows), std::move(column_names)};
    } catch (const std::exception& e) {
        return std::unexpected(ResultError{
            std::string("Error parsing results: ") + e.what()
        });
    }
}

} // namespace result
} // namespace relx

// Structured binding support
namespace std {
    template <typename... Types>
    struct tuple_size<relx::result::RowAdapter<Types...>> : 
        std::integral_constant<size_t, sizeof...(Types)> {};
    
    template <size_t I, typename... Types>
    struct tuple_element<I, relx::result::RowAdapter<Types...>> {
        using type = std::tuple_element_t<I, std::tuple<Types...>>;
    };
} 