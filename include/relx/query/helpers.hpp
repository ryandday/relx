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

} // namespace query
} // namespace relx 