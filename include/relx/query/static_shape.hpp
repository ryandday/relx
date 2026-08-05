#pragma once

#include "column_expression.hpp"
#include "condition.hpp"
#include "core.hpp"
#include "delete.hpp"
#include "insert.hpp"
#include "row_type.hpp"
#include "schema_adapter.hpp"
#include "select.hpp"
#include "update.hpp"
#include "value.hpp"

#include <optional>
#include <tuple>

/// @brief Static-shape detection: whether a query type's SQL text is fully determined
/// by its C++ type (independent of the values it holds). True for the core builder
/// vocabulary - column refs, placeholders (values always render "?", NULLs bind as
/// parameters), type-encoded operators and join types, and the query builders composed
/// of them. False by default for anything value-dependent: dynamic IN-lists (the
/// placeholder count is a runtime value), runtime-string aliases, and function/
/// arithmetic expressions whose name or operator is a runtime member.
///
/// Connections use this to build a query type's SQL exactly once per process (a
/// function-local static keyed on the type); it is also the gate a future
/// prepared-statement cache would use.
///
/// Assumption: optional clause slots (WHERE, HAVING, LIMIT, INSERT..SELECT) are
/// engaged-by-construction - the builder methods always store an engaged optional, so
/// the clause's presence is encoded in the type. Constructing a query directly with a
/// disengaged optional of engaged type would break this; the builders are the API.
namespace relx::query {

template <typename T>
inline constexpr bool has_static_shape_v = false;

// -- leaves ------------------------------------------------------------------

template <ColumnType C>
inline constexpr bool has_static_shape_v<ColumnRef<C>> = true;

template <ColumnType C>
inline constexpr bool has_static_shape_v<SchemaColumnAdapter<C>> = true;

template <typename T>
inline constexpr bool has_static_shape_v<Value<T>> = true;  // always renders "?"

template <>
inline constexpr bool has_static_shape_v<std::nullopt_t> = true;

template <>
inline constexpr bool has_static_shape_v<CountAllExpr> = true;

// -- composites: static iff their children are ------------------------------

template <typename... Ts>
inline constexpr bool has_static_shape_v<std::tuple<Ts...>> = (has_static_shape_v<Ts> && ...);

template <typename T>
inline constexpr bool has_static_shape_v<std::optional<T>> = has_static_shape_v<T>;

template <SqlExpr L, SqlExpr R, schema::fixed_string Op>
inline constexpr bool has_static_shape_v<BinaryCondition<L, R, Op>> = has_static_shape_v<L> &&
                                                                      has_static_shape_v<R>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<LikeCondition<E>> = has_static_shape_v<E>;

template <SqlExpr E, relx::ArrayBindElement T>
inline constexpr bool has_static_shape_v<AnyCondition<E, T>> = has_static_shape_v<E>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<BetweenCondition<E>> = has_static_shape_v<E>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<IsNullCondition<E>> = has_static_shape_v<E>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<IsNotNullCondition<E>> = has_static_shape_v<E>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<NotCondition<E>> = has_static_shape_v<E>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<AscendingExpr<E>> = has_static_shape_v<E>;

template <SqlExpr E>
inline constexpr bool has_static_shape_v<DescendingExpr<E>> = has_static_shape_v<E>;

template <schema::fixed_string Name, typename T, SqlExpr E>
inline constexpr bool has_static_shape_v<TypedAlias<Name, T, E>> = has_static_shape_v<E>;

template <TableType Table, ConditionExpr Condition, JoinType Type>
inline constexpr bool has_static_shape_v<JoinSpec<Table, Condition, Type>> =
    has_static_shape_v<Condition>;

template <ColumnType C, SqlExpr V>
inline constexpr bool has_static_shape_v<SetItem<C, V>> = has_static_shape_v<V>;

template <ColumnType C, SqlExpr V>
inline constexpr bool has_static_shape_v<InsertItem<C, V>> = has_static_shape_v<V>;

// -- query builders ----------------------------------------------------------

template <typename Columns, typename Tables, typename Joins, typename Where, typename GroupBys,
          typename OrderBys, typename HavingCond, typename LimitVal, typename OffsetVal,
          bool IsDistinct>
inline constexpr bool
    has_static_shape_v<SelectQuery<Columns, Tables, Joins, Where, GroupBys, OrderBys, HavingCond,
                                   LimitVal, OffsetVal, IsDistinct>> =
        has_static_shape_v<Columns> && has_static_shape_v<Joins> && has_static_shape_v<Where> &&
        has_static_shape_v<GroupBys> && has_static_shape_v<OrderBys> &&
        has_static_shape_v<HavingCond> && has_static_shape_v<LimitVal> &&
        has_static_shape_v<OffsetVal>;

template <TableType Table, typename Columns, typename Values, typename SelectStmt,
          typename ReturningColumns>
inline constexpr bool
    has_static_shape_v<InsertQuery<Table, Columns, Values, SelectStmt, ReturningColumns>> =
        has_static_shape_v<Columns> && has_static_shape_v<Values> &&
        has_static_shape_v<SelectStmt> && has_static_shape_v<ReturningColumns>;

template <TableType Table, typename Sets, typename Where, typename ReturningColumns>
inline constexpr bool has_static_shape_v<UpdateQuery<Table, Sets, Where, ReturningColumns>> =
    has_static_shape_v<Sets> && has_static_shape_v<Where> && has_static_shape_v<ReturningColumns>;

template <TableType Table, typename Where, typename ReturningColumns>
inline constexpr bool has_static_shape_v<DeleteQuery<Table, Where, ReturningColumns>> =
    has_static_shape_v<Where> && has_static_shape_v<ReturningColumns>;

}  // namespace relx::query

namespace relx {
using query::has_static_shape_v;
}  // namespace relx
