#pragma once

#include "core.hpp"
#include "column_expression.hpp"
#include "condition.hpp"
#include "schema_adapter.hpp"
#include "value.hpp"
#include <string>
#include <utility>
#include <concepts>
#include <type_traits>

namespace sqllib {
namespace query {

// Concepts to constrain template parameters
template <typename T>
concept SimpleValue = std::is_arithmetic_v<T> || 
                      std::convertible_to<T, std::string> ||
                      std::convertible_to<T, std::string_view>;

template <typename T>
concept SqlValue = std::same_as<T, Value<typename T::value_type>>;

// Specializations for various operand combinations

//
// EQUALITY OPERATORS (==)
//

/// @brief Overload for equality comparison between column and a simple value type
/// @tparam Col The column type
/// @tparam T The simple value type (numbers, strings)
/// @param col The column
/// @param value The value
/// @return A condition expression
template <ColumnType Col, SimpleValue T>
auto operator==(const Col& col, const T& value) {
    return to_expr(col) == val(value);
}

/// @brief Overload for equality comparison between column and a Value object
/// @tparam Col The column type
/// @tparam V The value type
/// @param col The column
/// @param value The value wrapper
/// @return A condition expression
template <ColumnType Col, typename V>
auto operator==(const Col& col, const Value<V>& value) {
    return to_expr(col) == value; // No need to wrap value again
}

/// @brief Overload for equality comparison between value and column
/// @tparam T The simple value type
/// @tparam Col The column type
/// @param value The value
/// @param col The column
/// @return A condition expression
template <SimpleValue T, ColumnType Col>
auto operator==(const T& value, const Col& col) {
    return val(value) == to_expr(col);
}

/// @brief Overload for equality comparison between a Value object and column
/// @tparam V The value type
/// @tparam Col The column type
/// @param value The value wrapper
/// @param col The column
/// @return A condition expression
template <typename V, ColumnType Col>
auto operator==(const Value<V>& value, const Col& col) {
    return value == to_expr(col); // No need to wrap value again
}

/// @brief Overload for equality comparison between two columns
/// @tparam Col1 The first column type
/// @tparam Col2 The second column type
/// @param col1 The first column
/// @param col2 The second column
/// @return A condition expression
template <ColumnType Col1, ColumnType Col2>
auto operator==(const Col1& col1, const Col2& col2) {
    return to_expr(col1) == to_expr(col2);
}

//
// INEQUALITY OPERATORS (!=)
//

/// @brief Overload for inequality comparison between column and a simple value type
/// @tparam Col The column type
/// @tparam T The simple value type (numbers, strings)
/// @param col The column
/// @param value The value
/// @return A condition expression
template <ColumnType Col, SimpleValue T>
auto operator!=(const Col& col, const T& value) {
    return to_expr(col) != val(value);
}

/// @brief Overload for inequality comparison between column and a Value object
/// @tparam Col The column type
/// @tparam V The value type
/// @param col The column
/// @param value The value wrapper
/// @return A condition expression
template <ColumnType Col, typename V>
auto operator!=(const Col& col, const Value<V>& value) {
    return to_expr(col) != value; // No need to wrap value again
}

/// @brief Overload for inequality comparison between value and column
/// @tparam T The simple value type
/// @tparam Col The column type
/// @param value The value
/// @param col The column
/// @return A condition expression
template <SimpleValue T, ColumnType Col>
auto operator!=(const T& value, const Col& col) {
    return val(value) != to_expr(col);
}

/// @brief Overload for inequality comparison between a Value object and column
/// @tparam V The value type
/// @tparam Col The column type
/// @param value The value wrapper
/// @param col The column
/// @return A condition expression
template <typename V, ColumnType Col>
auto operator!=(const Value<V>& value, const Col& col) {
    return value != to_expr(col); // No need to wrap value again
}

/// @brief Overload for inequality comparison between two columns
/// @tparam Col1 The first column type
/// @tparam Col2 The second column type
/// @param col1 The first column
/// @param col2 The second column
/// @return A condition expression
template <ColumnType Col1, ColumnType Col2>
auto operator!=(const Col1& col1, const Col2& col2) {
    return to_expr(col1) != to_expr(col2);
}

//
// GREATER THAN OPERATORS (>)
//

/// @brief Overload for greater than comparison between column and a simple value type
/// @tparam Col The column type
/// @tparam T The simple value type (numbers, strings)
/// @param col The column
/// @param value The value
/// @return A condition expression
template <ColumnType Col, SimpleValue T>
auto operator>(const Col& col, const T& value) {
    return to_expr(col) > val(value);
}

/// @brief Overload for greater than comparison between column and a Value object
/// @tparam Col The column type
/// @tparam V The value type
/// @param col The column
/// @param value The value wrapper
/// @return A condition expression
template <ColumnType Col, typename V>
auto operator>(const Col& col, const Value<V>& value) {
    return to_expr(col) > value; // No need to wrap value again
}

/// @brief Overload for greater than comparison between value and column
/// @tparam T The simple value type
/// @tparam Col The column type
/// @param value The value
/// @param col The column
/// @return A condition expression
template <SimpleValue T, ColumnType Col>
auto operator>(const T& value, const Col& col) {
    return val(value) > to_expr(col);
}

/// @brief Overload for greater than comparison between a Value object and column
/// @tparam V The value type
/// @tparam Col The column type
/// @param value The value wrapper
/// @param col The column
/// @return A condition expression
template <typename V, ColumnType Col>
auto operator>(const Value<V>& value, const Col& col) {
    return value > to_expr(col); // No need to wrap value again
}

/// @brief Overload for greater than comparison between two columns
/// @tparam Col1 The first column type
/// @tparam Col2 The second column type
/// @param col1 The first column
/// @param col2 The second column
/// @return A condition expression
template <ColumnType Col1, ColumnType Col2>
auto operator>(const Col1& col1, const Col2& col2) {
    return to_expr(col1) > to_expr(col2);
}

//
// LESS THAN OPERATORS (<)
//

/// @brief Overload for less than comparison between column and a simple value type
/// @tparam Col The column type
/// @tparam T The simple value type (numbers, strings)
/// @param col The column
/// @param value The value
/// @return A condition expression
template <ColumnType Col, SimpleValue T>
auto operator<(const Col& col, const T& value) {
    return to_expr(col) < val(value);
}

/// @brief Overload for less than comparison between column and a Value object
/// @tparam Col The column type
/// @tparam V The value type
/// @param col The column
/// @param value The value wrapper
/// @return A condition expression
template <ColumnType Col, typename V>
auto operator<(const Col& col, const Value<V>& value) {
    return to_expr(col) < value; // No need to wrap value again
}

/// @brief Overload for less than comparison between value and column
/// @tparam T The simple value type
/// @tparam Col The column type
/// @param value The value
/// @param col The column
/// @return A condition expression
template <SimpleValue T, ColumnType Col>
auto operator<(const T& value, const Col& col) {
    return val(value) < to_expr(col);
}

/// @brief Overload for less than comparison between a Value object and column
/// @tparam V The value type
/// @tparam Col The column type
/// @param value The value wrapper
/// @param col The column
/// @return A condition expression
template <typename V, ColumnType Col>
auto operator<(const Value<V>& value, const Col& col) {
    return value < to_expr(col); // No need to wrap value again
}

/// @brief Overload for less than comparison between two columns
/// @tparam Col1 The first column type
/// @tparam Col2 The second column type
/// @param col1 The first column
/// @param col2 The second column
/// @return A condition expression
template <ColumnType Col1, ColumnType Col2>
auto operator<(const Col1& col1, const Col2& col2) {
    return to_expr(col1) < to_expr(col2);
}

//
// GREATER THAN OR EQUAL OPERATORS (>=)
//

/// @brief Overload for greater than or equal comparison between column and a simple value type
/// @tparam Col The column type
/// @tparam T The simple value type (numbers, strings)
/// @param col The column
/// @param value The value
/// @return A condition expression
template <ColumnType Col, SimpleValue T>
auto operator>=(const Col& col, const T& value) {
    return to_expr(col) >= val(value);
}

/// @brief Overload for greater than or equal comparison between column and a Value object
/// @tparam Col The column type
/// @tparam V The value type
/// @param col The column
/// @param value The value wrapper
/// @return A condition expression
template <ColumnType Col, typename V>
auto operator>=(const Col& col, const Value<V>& value) {
    return to_expr(col) >= value; // No need to wrap value again
}

/// @brief Overload for greater than or equal comparison between value and column
/// @tparam T The simple value type
/// @tparam Col The column type
/// @param value The value
/// @param col The column
/// @return A condition expression
template <SimpleValue T, ColumnType Col>
auto operator>=(const T& value, const Col& col) {
    return val(value) >= to_expr(col);
}

/// @brief Overload for greater than or equal comparison between a Value object and column
/// @tparam V The value type
/// @tparam Col The column type
/// @param value The value wrapper
/// @param col The column
/// @return A condition expression
template <typename V, ColumnType Col>
auto operator>=(const Value<V>& value, const Col& col) {
    return value >= to_expr(col); // No need to wrap value again
}

/// @brief Overload for greater than or equal comparison between two columns
/// @tparam Col1 The first column type
/// @tparam Col2 The second column type
/// @param col1 The first column
/// @param col2 The second column
/// @return A condition expression
template <ColumnType Col1, ColumnType Col2>
auto operator>=(const Col1& col1, const Col2& col2) {
    return to_expr(col1) >= to_expr(col2);
}

//
// LESS THAN OR EQUAL OPERATORS (<=)
//

/// @brief Overload for less than or equal comparison between column and a simple value type
/// @tparam Col The column type
/// @tparam T The simple value type (numbers, strings)
/// @param col The column
/// @param value The value
/// @return A condition expression
template <ColumnType Col, SimpleValue T>
auto operator<=(const Col& col, const T& value) {
    return to_expr(col) <= val(value);
}

/// @brief Overload for less than or equal comparison between column and a Value object
/// @tparam Col The column type
/// @tparam V The value type
/// @param col The column
/// @param value The value wrapper
/// @return A condition expression
template <ColumnType Col, typename V>
auto operator<=(const Col& col, const Value<V>& value) {
    return to_expr(col) <= value; // No need to wrap value again
}

/// @brief Overload for less than or equal comparison between value and column
/// @tparam T The simple value type
/// @tparam Col The column type
/// @param value The value
/// @param col The column
/// @return A condition expression
template <SimpleValue T, ColumnType Col>
auto operator<=(const T& value, const Col& col) {
    return val(value) <= to_expr(col);
}

/// @brief Overload for less than or equal comparison between a Value object and column
/// @tparam V The value type
/// @tparam Col The column type
/// @param value The value wrapper
/// @param col The column
/// @return A condition expression
template <typename V, ColumnType Col>
auto operator<=(const Value<V>& value, const Col& col) {
    return value <= to_expr(col); // No need to wrap value again
}

/// @brief Overload for less than or equal comparison between two columns
/// @tparam Col1 The first column type
/// @tparam Col2 The second column type
/// @param col1 The first column
/// @param col2 The second column
/// @return A condition expression
template <ColumnType Col1, ColumnType Col2>
auto operator<=(const Col1& col1, const Col2& col2) {
    return to_expr(col1) <= to_expr(col2);
}

} // namespace query
} // namespace sqllib 