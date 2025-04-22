#pragma once

#include "core.hpp"
#include "select.hpp"
#include "schema_adapter.hpp"
#include "column_expression.hpp"
#include "value.hpp"
#include "function.hpp"
#include "condition.hpp"
#include <type_traits>
#include <string>
#include <utility>

namespace relx {
namespace query {

/// @brief This file provides helper functions and template aliases to reduce the verbosity of relx's API.
/// This makes it more convenient to use schema columns directly without manual to_expr calls
/// and reduces the need for explicit val() calls.

/// @brief Create a SELECT query for schema columns with automatic adapter creation
/// This makes it more convenient to use schema columns directly without manual to_expr calls
/// @tparam Cols The schema column types
/// @param cols The schema columns to select
/// @return A SelectQuery object with the columns wrapped in SchemaColumnAdapter
// Note: Removed duplicate select function since one already exists in select.hpp
// template <ColumnType... Cols>
// auto select(const Cols&... cols) {
//     return SelectQuery<std::tuple<SchemaColumnAdapter<Cols>...>>(
//         std::tuple<SchemaColumnAdapter<Cols>...>(SchemaColumnAdapter<Cols>(cols)...)
//     );
// }

/// @brief FROM extension for schema tables with automatic adapter creation
/// @tparam Columns The tuple of column expressions
/// @tparam Table The schema table type
/// @param query The select query
/// @param table The schema table
/// @return A new SelectQuery with the table added to the FROM clause
template <typename Columns, TableType Table>
auto from(const SelectQuery<Columns>& query, const Table& table) {
    auto adapter = to_table(table);
    return query.template from<decltype(adapter)>(adapter);
}

/// @brief JOIN extension for schema tables with automatic adapter creation
/// @tparam Columns The tuple of column expressions
/// @tparam Tables The tuple of table adapters
/// @tparam Joins The tuple of join specifications
/// @tparam Where The where condition type
/// @tparam GroupBys The tuple of group by expressions
/// @tparam OrderBys The tuple of order by expressions
/// @tparam HavingCond The having condition type
/// @tparam LimitVal The limit value type
/// @tparam OffsetVal The offset value type
/// @tparam Table The schema table type
/// @tparam Condition The join condition type
/// @param query The select query
/// @param table The schema table to join
/// @param cond The join condition
/// @param type The join type
/// @return A new SelectQuery with the join added
template <
    typename Columns, typename Tables, typename Joins,
    typename Where, typename GroupBys, typename OrderBys,
    typename HavingCond, typename LimitVal, typename OffsetVal,
    TableType Table, ConditionExpr Condition
>
auto join(
    const SelectQuery<Columns, Tables, Joins, Where, GroupBys, OrderBys, HavingCond, LimitVal, OffsetVal>& query,
    const Table& table,
    Condition cond,
    JoinType type = JoinType::Inner
) {
    auto adapter = to_table(table);
    return query.template join<decltype(adapter), Condition>(adapter, std::move(cond), type);
}

/// @brief Shorthand for converting a value to a SQL value
/// @tparam T The value type
/// @param v The value to convert
/// @return A Value expression representing the value
template <typename T>
inline auto v(T&& v) {
    return val(std::forward<T>(v));
}

/// @brief Shorthand for converting a column to a SQL expression
/// @tparam C The column type
/// @param c The column to convert
/// @return A column expression
template <ColumnType C>
inline auto e(const C& c) {
    return to_expr(c);
}

/// @brief Create a SQL expression for a column from a member pointer
/// @tparam MemberPtr Pointer to a column member
/// @return A column expression
template <auto MemberPtr>
inline auto e() {
    return to_expr<MemberPtr>();
}

/// @brief Convenient alias for 'as' to create column aliases
/// @tparam Expr The expression type
/// @param expr The expression to alias
/// @param alias The alias name
/// @return An aliased column expression
template <SqlExpr Expr>
inline auto a(const Expr& expr, std::string alias) {
    return as(expr, std::move(alias));
}

/// @brief Convenient alias for creating a LIKE condition
/// @param expr The expression to match
/// @param pattern The LIKE pattern
/// @return A LIKE condition expression
template <SqlExpr Expr>
inline auto l(const Expr& expr, std::string_view pattern) {
    return like(expr, pattern);
}

/// @brief Convenient alias for creating an IN condition
/// @tparam Expr The expression type
/// @tparam Values The container type for values
/// @param expr The expression to check
/// @param values The values to check against
/// @return An IN condition expression
template <SqlExpr Expr, typename Values>
inline auto i(const Expr& expr, const Values& values) {
    return in(expr, values);
}

/// @brief Convenient alias for creating a COUNT function
/// @tparam Expr The expression type
/// @param expr The expression to count
/// @return A COUNT function expression
template <SqlExpr Expr>
inline auto c(const Expr& expr) {
    return count(expr);
}

/// @brief Convenient alias for COUNT(*)
inline auto c_all() {
    return count_all();
}

/// @brief Convenient alias for COUNT(DISTINCT expr)
/// @tparam Expr The expression type
/// @param expr The expression to count distinct values of
/// @return A COUNT(DISTINCT expr) function expression
template <SqlExpr Expr>
inline auto c_distinct(const Expr& expr) {
    return count_distinct(expr);
}

/// @brief Convenient alias for creating a SUM function
/// @tparam Expr The expression type
/// @param expr The expression to sum
/// @return A SUM function expression
template <SqlExpr Expr>
inline auto s(const Expr& expr) {
    return sum(expr);
}

/// @brief Convenient alias for creating an AVG function
/// @tparam Expr The expression type
/// @param expr The expression to average
/// @return An AVG function expression
template <SqlExpr Expr>
inline auto a_avg(const Expr& expr) {
    return ::relx::query::avg(expr);
}

/// @brief Convenient alias for creating a MIN function
/// @tparam Expr The expression type
/// @param expr The expression to find the minimum of
/// @return A MIN function expression
template <SqlExpr Expr>
inline auto a_min(const Expr& expr) {
    return ::relx::query::min(expr);
}

/// @brief Convenient alias for creating a MAX function
/// @tparam Expr The expression type
/// @param expr The expression to find the maximum of
/// @return A MAX function expression
template <SqlExpr Expr>
inline auto a_max(const Expr& expr) {
    return ::relx::query::max(expr);
}

/// @brief Convenient alias for creating a DISTINCT expression
/// @tparam Expr The expression type
/// @param expr The expression to apply DISTINCT to
/// @return A DISTINCT expression
template <SqlExpr Expr>
inline auto d(const Expr& expr) {
    return distinct(expr);
}

/// @brief Convenient alias for creating an ascending order expression
/// @tparam Expr The expression type
/// @param expr The expression to order by
/// @return An ascending order expression
template <SqlExpr Expr>
inline auto a_by(const Expr& expr) {
    return asc(expr);
}

/// @brief Convenient alias for creating a descending order expression
/// @tparam Expr The expression type
/// @param expr The expression to order by
/// @return A descending order expression
template <SqlExpr Expr>
inline auto d_by(const Expr& expr) {
    return desc(expr);
}

} // namespace query
} // namespace relx 