#pragma once

#include "../reflect.hpp"
#include "../schema/annotated_table.hpp"
#include "column_expression.hpp"
#include "condition.hpp"
#include "core.hpp"
#include "meta.hpp"
#include "relx/schema/fixed_string.hpp"
#include "value.hpp"

#include <iostream>
#include <memory>
#include <meta>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace relx::query {

/// @brief relx Query Module
///
/// This module provides an API for creating SQL queries:
///
/// Instance-based API:
/// auto query = select(u.id, u.name).from(u);
/// Use columns directly in conditions.
///
/// @brief Join specification for a SELECT query. The join type is part of the type,
/// so a query's type fully determines its SQL text.
template <TableType Table, ConditionExpr Condition, JoinType Type = JoinType::Inner>
struct JoinSpec {
  using table_type = Table;
  Table table;
  Condition condition;
  static constexpr JoinType type = Type;
};

namespace detail {

/// @brief Whether a table type already appears in a FROM tuple
template <typename Table, typename Tuple>
inline constexpr bool tuple_contains_table_v = false;
template <typename Table, typename... Ts>
inline constexpr bool tuple_contains_table_v<Table, std::tuple<Ts...>> =
    (std::is_same_v<Table, Ts> || ...);

/// @brief Whether a table type already appears in a tuple of JoinSpecs
template <typename Table, typename Joins>
inline constexpr bool joins_contain_table_v = false;
template <typename Table, typename... Specs>
inline constexpr bool joins_contain_table_v<Table, std::tuple<Specs...>> =
    (std::is_same_v<Table, typename Specs::table_type> || ...);

/// @brief Whether a pack of types is pairwise distinct
template <typename... Ts>
inline constexpr bool all_distinct_v = true;
template <typename T, typename... Rest>
inline constexpr bool all_distinct_v<T, Rest...> = (!std::is_same_v<T, Rest> && ...) &&
                                                   all_distinct_v<Rest...>;

}  // namespace detail

/// @brief Create a join condition with the ON clause
/// @tparam Condition The condition type
/// @param cond The join condition
/// @return The condition expression
template <ConditionExpr Condition>
constexpr auto on(Condition cond) {
  return cond;
}

/// @brief Base SELECT query builder
/// @tparam Columns Tuple of column expressions
/// @tparam Tables Tuple of table references
/// @tparam Joins Tuple of join specifications
/// @tparam Where Optional where condition
/// @tparam GroupBys Tuple of group by expressions
/// @tparam OrderBys Tuple of order by expressions
/// @tparam HavingCond Optional having condition
/// @tparam LimitVal Optional limit value
/// @tparam OffsetVal Optional offset value
/// @tparam IsDistinct Whether to use DISTINCT in the query
template <typename Columns, typename Tables = std::tuple<>, typename Joins = std::tuple<>,
          typename Where = std::nullopt_t, typename GroupBys = std::tuple<>,
          typename OrderBys = std::tuple<>, typename HavingCond = std::nullopt_t,
          typename LimitVal = std::nullopt_t, typename OffsetVal = std::nullopt_t,
          bool IsDistinct = false>
class SelectQuery {
private:
public:
  /// Marker for SelectQueryExpr (subquery usage in conditions)
  using is_select_query = void;
  using columns_type = Columns;
  using tables_type = Tables;
  using joins_type = Joins;
  using where_type = Where;
  using group_bys_type = GroupBys;
  using order_bys_type = OrderBys;
  using having_type = HavingCond;
  using limit_type = LimitVal;
  using offset_type = OffsetVal;

  /// @brief Constructor for the SELECT query builder
  /// @param columns The columns to select
  /// @param tables The tables to select from
  /// @param joins The join specifications
  /// @param where The WHERE condition
  /// @param group_bys The GROUP BY expressions
  /// @param order_bys The ORDER BY expressions
  /// @param having The HAVING condition
  /// @param limit The LIMIT value
  /// @param offset The OFFSET value
  constexpr explicit SelectQuery(Columns columns, Tables tables = {}, Joins joins = {},
                                 Where where = std::nullopt, GroupBys group_bys = {},
                                 OrderBys order_bys = {}, HavingCond having = std::nullopt,
                                 LimitVal limit = std::nullopt, OffsetVal offset = std::nullopt)
      : columns_(std::move(columns)), tables_(std::move(tables)), joins_(std::move(joins)),
        where_(std::move(where)), group_bys_(std::move(group_bys)),
        order_bys_(std::move(order_bys)), having_(std::move(having)), limit_(std::move(limit)),
        offset_(std::move(offset)) {}

  /// @brief Generate the SQL for this SELECT query
  /// @return The SQL string
  constexpr std::string to_sql() const {
    std::string out = "SELECT ";

    // Add DISTINCT if enabled
    if constexpr (IsDistinct) {
      out += "DISTINCT ";
    }

    // Add columns
    out += tuple_to_sql(columns_, ", ");

    // Add FROM clause if tables are specified
    if constexpr (!is_empty_tuple<Tables>()) {
      out += " FROM ";
      int i = 0;
      std::apply(
          [&](const auto&... tables) {
            ((out += (i++ > 0 ? ", " : ""), out += tables.table_name), ...);
          },
          tables_);
    }

    // Add JOINs
    if constexpr (!is_empty_tuple<Joins>()) {
      apply_tuple(
          [&](const auto& join) {
            out += " ";
            switch (join.type) {
            case JoinType::Inner:
              out += "JOIN ";
              break;
            case JoinType::Left:
              out += "LEFT JOIN ";
              break;
            case JoinType::Right:
              out += "RIGHT JOIN ";
              break;
            case JoinType::Full:
              out += "FULL JOIN ";
              break;
            case JoinType::Cross:
              out += "CROSS JOIN ";
              break;
            }
            out += join.table.table_name;

            if (join.type != JoinType::Cross) {
              out += " ON ";
              out += join.condition.to_sql();
            }
          },
          joins_);
    }

    // Add WHERE clause
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        out += " WHERE " + where_.value().to_sql();
      }
    }

    // Add GROUP BY clause
    if constexpr (!is_empty_tuple<GroupBys>()) {
      out += " GROUP BY " + tuple_to_sql(group_bys_, ", ");
    }

    // Add HAVING clause
    if constexpr (!std::is_same_v<HavingCond, std::nullopt_t>) {
      if (having_.has_value()) {
        out += " HAVING " + having_.value().to_sql();
      }
    }

    // Add ORDER BY clause
    if constexpr (!is_empty_tuple<OrderBys>()) {
      out += " ORDER BY " + tuple_to_sql(order_bys_, ", ");
    }

    // Add LIMIT clause
    if constexpr (!std::is_same_v<LimitVal, std::nullopt_t>) {
      if constexpr (std::is_same_v<LimitVal, Value<int>>) {
        out += " LIMIT " + limit_.to_sql();
      } else if (limit_.has_value()) {
        out += " LIMIT " + limit_.value().to_sql();
      }
    }

    // Add OFFSET clause
    if constexpr (!std::is_same_v<OffsetVal, std::nullopt_t>) {
      if constexpr (std::is_same_v<OffsetVal, Value<int>>) {
        out += " OFFSET " + offset_.to_sql();
      } else if (offset_.has_value()) {
        out += " OFFSET " + offset_.value().to_sql();
      }
    }

    return out;
  }

  /// @brief Get the bind parameters for this SELECT query
  /// @return Vector of bind parameters
  constexpr std::vector<bind_param> bind_params() const {
    std::vector<bind_param> params;

    // Collect parameters from columns
    if constexpr (!is_empty_tuple<Columns>()) {
      auto column_params = tuple_bind_params(columns_);
      params.insert(params.end(), column_params.begin(), column_params.end());
    }

    // Tables don't have bind parameters, so we skip collecting them

    // Collect parameters from joins
    if constexpr (!is_empty_tuple<Joins>()) {
      std::apply(
          [&](const auto&... joins) {
            (([&]() {
               try {
                 auto join_params = joins.condition.bind_params();
                 params.insert(params.end(), join_params.begin(), join_params.end());
               } catch (const std::exception& e) {
                 // Log error and continue
                 std::print("Error collecting join params: {}", e.what());
               }
             })(),
             ...);
          },
          joins_);
    }

    // Collect where parameter
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        auto where_params = where_.value().bind_params();
        params.insert(params.end(), where_params.begin(), where_params.end());
      }
    }

    // Collect group by parameters
    if constexpr (!is_empty_tuple<GroupBys>()) {
      auto group_params = tuple_bind_params(group_bys_);
      params.insert(params.end(), group_params.begin(), group_params.end());
    }

    // Collect having parameter
    if constexpr (!std::is_same_v<HavingCond, std::nullopt_t>) {
      if (having_.has_value()) {
        auto having_params = having_.value().bind_params();
        params.insert(params.end(), having_params.begin(), having_params.end());
      }
    }

    // Collect order by parameters
    if constexpr (!is_empty_tuple<OrderBys>()) {
      auto order_params = tuple_bind_params(order_bys_);
      params.insert(params.end(), order_params.begin(), order_params.end());
    }

    // Collect limit parameter
    if constexpr (!std::is_same_v<LimitVal, std::nullopt_t>) {
      if constexpr (std::is_same_v<LimitVal, Value<int>>) {
        auto limit_params = limit_.bind_params();
        params.insert(params.end(), limit_params.begin(), limit_params.end());
      } else if (limit_.has_value()) {
        auto limit_params = limit_.value().bind_params();
        params.insert(params.end(), limit_params.begin(), limit_params.end());
      }
    }

    // Collect offset parameter
    if constexpr (!std::is_same_v<OffsetVal, std::nullopt_t>) {
      if constexpr (std::is_same_v<OffsetVal, Value<int>>) {
        auto offset_params = offset_.bind_params();
        params.insert(params.end(), offset_params.begin(), offset_params.end());
      } else if (offset_.has_value()) {
        auto offset_params = offset_.value().bind_params();
        params.insert(params.end(), offset_params.begin(), offset_params.end());
      }
    }

    return params;
  }

  /// @brief Add a FROM clause to the query
  /// @tparam T The table type
  /// @param table The table to select from
  /// @return New SelectQuery with the FROM clause added
  template <TableType T>
  constexpr auto from(const T& table) const {
    static_assert(!detail::tuple_contains_table_v<T, Tables>,
                  "duplicate table in FROM: relx has no table aliases, and PostgreSQL rejects the "
                  "same table name appearing twice without an alias");
    using new_tables_type = decltype(std::tuple_cat(tables_, std::tuple<T>()));
    return SelectQuery<Columns, new_tables_type, Joins, Where, GroupBys, OrderBys, HavingCond,
                       LimitVal, OffsetVal, IsDistinct>(
        columns_, std::tuple_cat(tables_, std::tuple<T>(table)), joins_, where_, group_bys_,
        order_bys_, having_, limit_, offset_);
  }

  /// @brief Add a FROM clause with multiple tables
  /// @tparam Args The table types
  /// @param tables The tables to select from
  /// @return New SelectQuery with the FROM clause added
  template <TableType... Args>
  constexpr auto from(const Args&... tables) const {
    static_assert((!detail::tuple_contains_table_v<Args, Tables> && ...) &&
                      detail::all_distinct_v<Args...>,
                  "duplicate table in FROM: relx has no table aliases, and PostgreSQL rejects the "
                  "same table name appearing twice without an alias");
    using new_tables_type = decltype(std::tuple_cat(tables_, std::tuple<Args...>()));
    return SelectQuery<Columns, new_tables_type, Joins, Where, GroupBys, OrderBys, HavingCond,
                       LimitVal, OffsetVal, IsDistinct>(
        columns_, std::tuple_cat(tables_, std::tuple<Args...>(tables...)), joins_, where_,
        group_bys_, order_bys_, having_, limit_, offset_);
  }

  /// @brief Add a JOIN clause to the query
  /// @tparam Table The table type to join
  /// @tparam Condition The join condition type
  /// @param table The table to join
  /// @param cond The join condition
  /// @param type The join type (default: INNER)
  /// @return New SelectQuery with the JOIN clause added
  template <JoinType Type = JoinType::Inner, TableType Table, ConditionExpr Condition>
  constexpr auto join(const Table& table, const Condition& cond) const {
    static_assert(!detail::tuple_contains_table_v<Table, Tables> &&
                      !detail::joins_contain_table_v<Table, Joins>,
                  "self-join is not supported: relx has no table aliases, and PostgreSQL rejects "
                  "the same table name appearing twice without an alias");
    using Spec = JoinSpec<Table, Condition, Type>;
    using new_joins_type = decltype(std::tuple_cat(joins_, std::tuple<Spec>{Spec{table, cond}}));

    return SelectQuery<Columns, Tables, new_joins_type, Where, GroupBys, OrderBys, HavingCond,
                       LimitVal, OffsetVal, IsDistinct>(
        columns_, tables_, std::tuple_cat(joins_, std::tuple<Spec>{Spec{table, cond}}), where_,
        group_bys_, order_bys_, having_, limit_, offset_);
  }

  /// @brief Add a LEFT JOIN clause to the query
  /// @tparam Table The table type to join
  /// @tparam Condition The join condition type
  /// @param table The table to join
  /// @param cond The join condition
  /// @return New SelectQuery with the LEFT JOIN clause added
  template <TableType Table, ConditionExpr Condition>
  constexpr auto left_join(const Table& table, const Condition& cond) const {
    return join<JoinType::Left>(table, cond);
  }

  /// @brief Add a RIGHT JOIN clause to the query
  /// @tparam Table The table type to join
  /// @tparam Condition The join condition type
  /// @param table The table to join
  /// @param cond The join condition
  /// @return New SelectQuery with the RIGHT JOIN clause added
  template <TableType Table, ConditionExpr Condition>
  constexpr auto right_join(const Table& table, const Condition& cond) const {
    return join<JoinType::Right>(table, cond);
  }

  /// @brief Add a FULL JOIN clause to the query
  /// @tparam Table The table type to join
  /// @tparam Condition The join condition type
  /// @param table The table to join
  /// @param cond The join condition
  /// @return New SelectQuery with the FULL JOIN clause added
  template <TableType Table, ConditionExpr Condition>
  constexpr auto full_join(const Table& table, const Condition& cond) const {
    return join<JoinType::Full>(table, cond);
  }

  /// @brief Add a CROSS JOIN clause to the query
  /// @tparam Table The table type to join
  /// @param table The table to join
  /// @return New SelectQuery with the CROSS JOIN clause added
  template <TableType Table>
  constexpr auto cross_join(const Table& table) const {
    // A cross join doesn't need a condition, so we use a dummy condition
    struct DummyCondition : public SqlExpression {
      // TODO Get rid of this dummy conditions somehow
      constexpr std::string to_sql() const override { return "1=1"; }
      constexpr std::vector<bind_param> bind_params() const override { return {}; }
    };

    return join<JoinType::Cross>(table, DummyCondition{});
  }

  /// @brief Add a WHERE clause to the query
  /// @tparam Condition The condition type
  /// @param cond The WHERE condition
  /// @return New SelectQuery with the WHERE clause added
  template <ConditionExpr Condition>
  constexpr auto where(const Condition& cond) const {
    return SelectQuery<Columns, Tables, Joins, std::optional<Condition>, GroupBys, OrderBys,
                       HavingCond, LimitVal, OffsetVal, IsDistinct>(
        columns_, tables_, joins_, std::optional<Condition>(cond), group_bys_, order_bys_, having_,
        limit_, offset_);
  }

  /// @brief Add a GROUP BY clause to the query
  /// @tparam Args The group by expression types
  /// @param args The GROUP BY expressions
  /// @return New SelectQuery with the GROUP BY clause added
  template <typename... Args>
  constexpr auto group_by(const Args&... args) const {
    auto transform_arg = []<typename T>(const T& arg) {
      if constexpr (ColumnType<T>) {
        return to_expr(arg);
      } else {
        return arg;
      }
    };

    using new_group_bys_type = decltype(std::tuple_cat(
        group_bys_, std::make_tuple(transform_arg(std::declval<Args>())...)));

    return SelectQuery<Columns, Tables, Joins, Where, new_group_bys_type, OrderBys, HavingCond,
                       LimitVal, OffsetVal, IsDistinct>(
        columns_, tables_, joins_, where_,
        std::tuple_cat(group_bys_, std::make_tuple(transform_arg(args)...)), order_bys_, having_,
        limit_, offset_);
  }

  /// @brief Add a HAVING clause to the query
  /// @tparam Condition The condition type
  /// @param cond The HAVING condition
  /// @return New SelectQuery with the HAVING clause added
  template <ConditionExpr Condition>
  constexpr auto having(const Condition& cond) const {
    return SelectQuery<Columns, Tables, Joins, Where, GroupBys, OrderBys, std::optional<Condition>,
                       LimitVal, OffsetVal, IsDistinct>(
        columns_, tables_, joins_, where_, group_bys_, order_bys_, std::optional<Condition>(cond),
        limit_, offset_);
  }

  /// @brief Add an ORDER BY clause to the query
  /// @tparam Args The order by expression types
  /// @param args The ORDER BY expressions
  /// @return New SelectQuery with the ORDER BY clause added
  template <SqlExpr... Args>
  constexpr auto order_by(const Args&... args) const {
    using new_order_bys_type = decltype(std::tuple_cat(order_bys_, std::tuple<Args...>{args...}));

    return SelectQuery<Columns, Tables, Joins, Where, GroupBys, new_order_bys_type, HavingCond,
                       LimitVal, OffsetVal, IsDistinct>(
        columns_, tables_, joins_, where_, group_bys_,
        std::tuple_cat(order_bys_, std::tuple<Args...>{args...}), having_, limit_, offset_);
  }

  /// @brief Add an ORDER BY clause to the query
  /// @tparam T The column type
  /// @param column The column to order by
  /// @return New SelectQuery with the ORDER BY clause added
  template <typename T>
    requires ColumnType<T>
  constexpr auto order_by(const T& column) const {
    // Type check for ORDER BY - ensure the column is comparable
    // Extract column type from the column template parameters
    using ColumnType = typename T::value_type;
    static_assert(std::totally_ordered<ColumnType> && !std::same_as<ColumnType, bool>,
                  "ORDER BY can only be used with comparable columns (numeric types, strings, "
                  "timestamps, uuids). Boolean columns or non-comparable types cannot be used for "
                  "ordering.");

    return order_by(asc(to_expr(column)));
  }

  /// @brief Add a LIMIT clause to the query
  /// @param limit The LIMIT value
  /// @return New SelectQuery with the LIMIT clause added
  constexpr auto limit(int limit) const {
    auto limit_expr = Value<int>(limit);

    return SelectQuery<Columns, Tables, Joins, Where, GroupBys, OrderBys, HavingCond,
                       decltype(limit_expr), OffsetVal, IsDistinct>(
        columns_, tables_, joins_, where_, group_bys_, order_bys_, having_, std::move(limit_expr),
        offset_);
  }

  /// @brief Add an OFFSET clause to the query
  /// @param offset The OFFSET value
  /// @return New SelectQuery with the OFFSET clause added
  constexpr auto offset(int offset) const {
    auto offset_expr = Value<int>(offset);

    return SelectQuery<Columns, Tables, Joins, Where, GroupBys, OrderBys, HavingCond, LimitVal,
                       decltype(offset_expr), IsDistinct>(columns_, tables_, joins_, where_,
                                                          group_bys_, order_bys_, having_, limit_,
                                                          std::move(offset_expr));
  }

private:
  Columns columns_;
  Tables tables_;
  Joins joins_;
  Where where_;
  GroupBys group_bys_;
  OrderBys order_bys_;
  HavingCond having_;
  LimitVal limit_;
  OffsetVal offset_;
};

/// @brief Create a column reference from a member pointer without requiring a table instance
/// @tparam MemberPtr Pointer to a column member in a table class

/// @brief Create a SELECT query with the specified columns or expressions
/// @tparam Args The column or expression types
/// @param args The columns or expressions to select
/// @return A SelectQuery object
template <typename... Args>
constexpr auto select(const Args&... args) {
  static_assert((!SelectQueryExpr<Args> && ...),
                "scalar subqueries in a select list are not supported: the nested SELECT would be "
                "emitted without parentheses. Use an IN/EXISTS condition, or a join");
  if constexpr ((ColumnType<Args> && ...)) {
    // All arguments are columns
    return SelectQuery(std::tuple<ColumnRef<Args>...>(ColumnRef<Args>(args)...));
  } else if constexpr ((SqlExpr<Args> && ...)) {
    // All arguments are expressions
    return SelectQuery(std::tuple<Args...>(args...));
  } else {
    // Mixed columns and expressions
    // Use a helper function to convert columns to ColumnRef and leave expressions as is
    auto transform_arg = []<typename T>(const T& arg) {
      if constexpr (ColumnType<T>) {
        return ColumnRef<T>(arg);
      } else {
        return arg;
      }
    };

    return SelectQuery(std::make_tuple(transform_arg(args)...));
  }
}

/// @brief Create a SELECT query with the specified column expressions (alias for select)
/// @tparam Args The column expression types
/// @param args The column expressions to select
/// @return A SelectQuery object
template <typename... Args>
constexpr auto select_expr(const Args&... args) {
  return select(args...);
}

/// @brief Helper for creating a descending order by expression
/// @tparam Expr The expression type
/// @param expr The expression to order by
/// @return An expression that generates "expr DESC"
template <SqlExpr Expr>
class DescendingExpr : public SqlExpression {
public:
  constexpr explicit DescendingExpr(Expr expr) : expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " DESC"; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

private:
  Expr expr_;
};

/// @brief Create a descending order by expression
/// @tparam Expr The expression type
/// @param expr The expression to order by
/// @return A DescendingExpr object
template <SqlExpr Expr>
constexpr auto desc(Expr expr) {
  return DescendingExpr<Expr>(std::move(expr));
}

template <typename T>
  requires ColumnType<T>
constexpr auto desc(const T& column) {
  return desc(to_expr(column));
}

/// @brief Helper for creating an ascending order by expression
/// @tparam Expr The expression type
/// @param expr The expression to order by
/// @return An expression that generates "expr ASC"
template <SqlExpr Expr>
class AscendingExpr : public SqlExpression {
public:
  constexpr explicit AscendingExpr(Expr expr) : expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " ASC"; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

private:
  Expr expr_;
};

/// @brief Create an ascending order by expression
/// @tparam Expr The expression type
/// @param expr The expression to order by
/// @return An AscendingExpr object
template <SqlExpr Expr>
constexpr auto asc(Expr expr) {
  return AscendingExpr<Expr>(std::move(expr));
}

/// @brief Create an ascending order by expression for a column
/// @tparam T The column type
/// @param column The column to order by
/// @return An AscendingExpr object
template <typename T>
  requires ColumnType<T>
constexpr auto asc(const T& column) {
  return asc(to_expr(column));
}

// clang-format off
// (select_all_columns lives in meta.hpp, shared with returning_all)

/// @brief Create a SELECT query over every column of the table, expanded to an explicit
/// column list via reflection. No raw `*`: the statement is stable under column
/// additions/reorderings between compile and execution, and the query synthesizes a row
/// type like any explicit select (fetch_all/fetch_one work).
template <TableType Table>
constexpr auto select_all(const Table& table) {
  static_assert(detail::select_all_columns<Table>().size() > 0,
                "select_all requires the table to have at least one column");
  return [&table]<std::size_t... I>(std::index_sequence<I...>) {
    return select(table.[:detail::select_all_columns<Table>()[I]:]...).from(table);
  }(std::make_index_sequence<detail::select_all_columns<Table>().size()>{});
}

// clang-format on

/// @brief Create a select-all query without requiring a table instance
/// @tparam Table The table type to select all columns from
/// @return A SelectQuery object with all columns from the table
template <TableType Table>
constexpr auto select_all() {
  // Function-local static: the returned query's column refs point at this instance
  static const Table table{};
  return select_all(table);
}

/// @brief select_all over an annotated struct directly: select_all<Users>()
template <typename Table>
  requires(!TableType<Table>)
constexpr auto select_all() {
  return select_all<schema::table_ref<Table>>();
}

// Add new helper functions for DISTINCT queries

/// @brief Create a SELECT DISTINCT query with the specified columns or expressions
/// @tparam Args The column or expression types
/// @param args The columns or expressions to select
/// @return A SelectQuery object with DISTINCT enabled
template <typename... Args>
constexpr auto select_distinct(const Args&... args) {
  static_assert((!SelectQueryExpr<Args> && ...),
                "scalar subqueries in a select list are not supported: the nested SELECT would be "
                "emitted without parentheses. Use an IN/EXISTS condition, or a join");
  if constexpr ((ColumnType<Args> && ...)) {
    // All arguments are columns
    return SelectQuery<std::tuple<ColumnRef<Args>...>, std::tuple<>, std::tuple<>, std::nullopt_t,
                       std::tuple<>, std::tuple<>, std::nullopt_t, std::nullopt_t, std::nullopt_t,
                       true>(std::tuple<ColumnRef<Args>...>(ColumnRef<Args>(args)...));
  } else if constexpr ((SqlExpr<Args> && ...)) {
    // All arguments are expressions
    return SelectQuery<std::tuple<Args...>, std::tuple<>, std::tuple<>, std::nullopt_t,
                       std::tuple<>, std::tuple<>, std::nullopt_t, std::nullopt_t, std::nullopt_t,
                       true>(std::tuple<Args...>(args...));
  } else {
    // Mixed columns and expressions
    auto transform_arg = []<typename T>(const T& arg) {
      if constexpr (ColumnType<T>) {
        return ColumnRef<T>(arg);
      } else {
        return arg;
      }
    };

    return SelectQuery<std::tuple<decltype(transform_arg(std::declval<Args>()))...>, std::tuple<>,
                       std::tuple<>, std::nullopt_t, std::tuple<>, std::tuple<>, std::nullopt_t,
                       std::nullopt_t, std::nullopt_t, true>(
        std::make_tuple(transform_arg(args)...));
  }
}

/// @brief Create a SELECT DISTINCT query with the specified column expressions
/// @tparam Args The column expression types
/// @param args The column expressions to select
/// @return A SelectQuery object with DISTINCT enabled
template <SqlExpr... Args>
auto select_distinct_expr(const Args&... args) {
  return select_distinct(args...);
}

/// @brief Create a SELECT * query that uses DISTINCT
/// @tparam Table The table type to select all columns from
/// @param table The table to select from
/// @return A SelectQuery object with all columns from the table and DISTINCT enabled
template <TableType Table>
auto select_distinct_all(const Table& table) {
  // Create a SelectQuery that selects DISTINCT *
  class StarExpression : public SqlExpression {
  public:
    constexpr std::string to_sql() const override { return "*"; }

    constexpr std::vector<bind_param> bind_params() const override { return {}; }
  };

  // Use a star expression with DISTINCT
  return SelectQuery<std::tuple<StarExpression>, std::tuple<>, std::tuple<>, std::nullopt_t,
                     std::tuple<>, std::tuple<>, std::nullopt_t, std::nullopt_t, std::nullopt_t,
                     true>(std::tuple<StarExpression>())
      .from(table);
}

/// @brief Create a SELECT DISTINCT * query without requiring a table instance
/// @tparam Table The table type to select distinct all columns from
/// @return A SelectQuery object with all columns from the table and DISTINCT enabled
template <TableType Table>
auto select_distinct_all() {
  // Create a table instance to work with
  Table table{};
  return select_distinct_all(table);
}

/// @brief select_distinct_all over an annotated struct directly: select_distinct_all<Users>()
template <typename Table>
  requires(!TableType<Table>)
auto select_distinct_all() {
  return select_distinct_all<schema::table_ref<Table>>();
}

}  // namespace relx::query
