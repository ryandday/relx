#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <concepts>
#include <type_traits>
#include <utility>
#include <ranges>
#include <expected>

#include "../schema/core.hpp"
#include "../schema/column.hpp"
#include "../schema/table.hpp"

namespace sqllib {
namespace query {

/// @brief Error type for query operations
struct QueryError {
    std::string message;
};

/// @brief Type alias for result of query operations
template <typename T>
using QueryResult = std::expected<T, QueryError>;

/// @brief Concept for SQL expression components
template <typename T>
concept SqlExpr = requires(T t) {
    { t.to_sql() } -> std::convertible_to<std::string>;
    { t.bind_params() } -> std::ranges::range;
};

/// @brief Concept for database table types
template <typename T>
concept TableType = schema::TableConcept<T>;

/// @brief Concept for column types
template <typename T>
concept ColumnType = requires(T t) {
    { T::name } -> std::convertible_to<std::string_view>;
    typename T::value_type;
};

/// @brief Concept for a sequence of column references
template <typename T>
concept ColumnList = std::ranges::range<T> && 
    requires { typename std::ranges::range_value_t<T>; } &&
    ColumnType<std::remove_cvref_t<std::ranges::range_value_t<T>>>;

/// @brief Concept for a sequence of table references
template <typename T>
concept TableList = std::ranges::range<T> && 
    requires { typename std::ranges::range_value_t<T>; } &&
    TableType<std::remove_cvref_t<std::ranges::range_value_t<T>>>;

/// @brief Concept for a condition expression
template <typename T>
concept ConditionExpr = SqlExpr<T>;

/// @brief Base class for SQL expressions
struct SqlExpression {
    virtual ~SqlExpression() = default;
    virtual std::string to_sql() const = 0;
    virtual std::vector<std::string> bind_params() const = 0;
};

/// @brief Types of JOIN operations
enum class JoinType {
    Inner,
    Left,
    Right,
    Full,
    Cross
};

/// @brief Convert a JoinType to its SQL string representation
inline std::string join_type_to_string(JoinType type) {
    switch (type) {
        case JoinType::Inner: return "INNER JOIN";
        case JoinType::Left:  return "LEFT JOIN";
        case JoinType::Right: return "RIGHT JOIN";
        case JoinType::Full:  return "FULL JOIN";
        case JoinType::Cross: return "CROSS JOIN";
        default:              return "JOIN";
    }
}

} // namespace query
} // namespace sqllib 