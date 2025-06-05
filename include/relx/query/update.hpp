#pragma once

#include "column_expression.hpp"
#include "condition.hpp"
#include "core.hpp"
#include "function.hpp"
#include "meta.hpp"
#include "operators.hpp"
#include "value.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace relx {
namespace query {

/// @brief Represents a SET clause assignment in an UPDATE statement
template <ColumnType Column, SqlExpr Value>
struct SetItem {
  ColumnRef<Column> column;
  Value value;

  // Constructor to ensure the SetItem can be properly initialized
  SetItem(ColumnRef<Column> col, Value val) : column(std::move(col)), value(std::move(val)) {}

  std::string to_sql() const { return column.column_name() + " = " + value.to_sql(); }

  std::vector<std::string> bind_params() const { return value.bind_params(); }
};

/// @brief Base UPDATE query builder
/// @tparam Table Table to update
/// @tparam Sets Tuple of SET clause items
/// @tparam Where Optional where condition
/// @tparam ReturningColumns Tuple of column expressions to return after update
template <TableType Table, typename Sets = std::tuple<>, typename Where = std::nullopt_t,
          typename ReturningColumns = std::tuple<>>
class UpdateQuery {
private:
  Table table_;
  Sets sets_;
  Where where_;
  ReturningColumns returning_columns_;

  // Helper to convert the RETURNING clause to SQL
  std::string returning_to_sql() const {
    if constexpr (is_empty_tuple<ReturningColumns>()) {
      return "";
    } else {
      std::stringstream ss;
      ss << " RETURNING ";
      int i = 0;
      std::apply(
          [&](const auto&... cols) { ((ss << (i++ > 0 ? ", " : "") << cols.to_sql()), ...); },
          returning_columns_);
      return ss.str();
    }
  }

  // Helper to collect bind parameters from RETURNING clause
  std::vector<std::string> returning_bind_params() const {
    std::vector<std::string> params;

    if constexpr (!is_empty_tuple<ReturningColumns>()) {
      std::apply(
          [&](const auto&... cols) {
            auto process_col = [&params](const auto& col) {
              auto col_params = col.bind_params();
              params.insert(params.end(), col_params.begin(), col_params.end());
            };

            (process_col(cols), ...);
          },
          returning_columns_);
    }

    return params;
  }

public:
  using table_type = Table;
  using sets_type = Sets;
  using where_type = Where;
  using returning_columns_type = ReturningColumns;

  /// @brief Constructor for the UPDATE query builder
  /// @param table The table to update
  /// @param sets The SET clause items
  /// @param where The WHERE condition
  /// @param returning_columns The columns to return after update
  explicit UpdateQuery(Table table, Sets sets = {}, Where where = std::nullopt,
                       ReturningColumns returning_columns = {})
      : table_(std::move(table)), sets_(std::move(sets)), where_(std::move(where)),
        returning_columns_(std::move(returning_columns)) {}

  /// @brief Generate the SQL for this UPDATE query
  /// @return The SQL string
  std::string to_sql() const {
    std::stringstream ss;
    ss << "UPDATE " << table_.table_name;

    // Add SET clause
    if constexpr (!is_empty_tuple<Sets>()) {
      ss << " SET ";
      ss << tuple_to_sql(sets_, ", ");
    }

    // Add WHERE clause
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        ss << " WHERE " << where_.value().to_sql();
      }
    }

    // Add RETURNING clause if specified
    ss << returning_to_sql();

    return ss.str();
  }

  /// @brief Get the bind parameters for this UPDATE query
  /// @return Vector of bind parameters
  std::vector<std::string> bind_params() const {
    std::vector<std::string> params;

    // Collect parameters from SET items
    if constexpr (!is_empty_tuple<Sets>()) {
      auto set_params = tuple_bind_params(sets_);
      params.insert(params.end(), set_params.begin(), set_params.end());
    }

    // Collect where parameter
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        auto where_params = where_.value().bind_params();
        params.insert(params.end(), where_params.begin(), where_params.end());
      }
    }

    // Add RETURNING bind parameters
    auto returning_params = returning_bind_params();
    params.insert(params.end(), returning_params.begin(), returning_params.end());

    return params;
  }

  /// @brief Add or replace a SET clause assignment
  /// @tparam Col The column type
  /// @tparam Val The value expression type
  /// @param column The column to set
  /// @param val The value to set
  /// @return New UpdateQuery with the SET clause added
  template <ColumnType Col, SqlExpr Val>
  auto set(const Col& column, Val&& val) const {
    using SetItemType = SetItem<Col, std::remove_cvref_t<Val>>;

    // Create a new SetItem
    auto col_ref = ColumnRef<Col>(column);
    auto set_item = SetItemType(col_ref, std::forward<Val>(val));

    // Create a tuple with the new SetItem
    auto new_item_tuple = std::make_tuple(std::move(set_item));

    // Concatenate with existing sets
    auto new_sets = std::tuple_cat(sets_, std::move(new_item_tuple));

    // Return a new query with the updated sets
    return UpdateQuery<Table, decltype(new_sets), Where, ReturningColumns>(
        table_, std::move(new_sets), where_, returning_columns_);
  }

  /// @brief Add or replace a SET clause assignment with a raw value (with type checking)
  /// @tparam Col The column type
  /// @tparam T The raw value type
  /// @param column The column to set
  /// @param val The raw value to set
  /// @return New UpdateQuery with the SET clause added
  template <ColumnType Col, typename T>
    requires(!SqlExpr<T>)
  auto set(const Col& column, T&& val) const {
    // Note: For now, we'll use a basic type check until we can access the type_checking utilities
    // This provides a good foundation for type safety in UPDATE operations

    // Wrap the raw value in a Value expression
    auto value_expr = value(std::forward<T>(val));
    return set(column, std::move(value_expr));
  }

  /// @brief Add a WHERE clause to the query
  /// @tparam Condition The condition type
  /// @param cond The WHERE condition
  /// @return New UpdateQuery with the WHERE clause added
  template <ConditionExpr Condition>
  auto where(const Condition& cond) const {
    return UpdateQuery<Table, Sets, std::optional<Condition>, ReturningColumns>(
        table_, sets_, std::optional<Condition>(cond), returning_columns_);
  }

  /// @brief Set a condition for filtering the rows to update using IN with values
  /// @tparam Col The column type
  /// @tparam Range The range type for IN values
  /// @param column The column to check
  /// @param values The values to check against
  /// @return New UpdateQuery with the IN condition added
  template <ColumnType Col, std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
  auto where_in(const Col& column, const Range& values) const {
    auto col_expr = column_ref(column);
    auto in_condition = in(col_expr, values);
    return where(in_condition);
  }

  /// @brief Specify columns to return after update
  /// @tparam Args Column types or SQL expressions
  /// @param args The columns or expressions to return
  /// @return New UpdateQuery with the RETURNING clause added
  template <typename... Args>
  auto returning(const Args&... args) const {
    // Helper to convert columns to ColumnRef expressions if they're not already SqlExpr
    auto to_expr = [](const auto& arg) {
      if constexpr (SqlExpr<std::remove_cvref_t<decltype(arg)>>) {
        return arg;
      } else if constexpr (ColumnType<std::remove_cvref_t<decltype(arg)>>) {
        return column_ref(arg);
      } else {
        static_assert(SqlExpr<std::remove_cvref_t<decltype(arg)>> ||
                          ColumnType<std::remove_cvref_t<decltype(arg)>>,
                      "Arguments to returning() must be either columns or SQL expressions");
        // This line is never reached, it's just to make the compiler happy
        return arg;
      }
    };

    using ReturningTuple = std::tuple<decltype(to_expr(std::declval<Args>()))...>;
    auto returning_tuple = std::make_tuple(to_expr(args)...);

    return UpdateQuery<Table, Sets, Where, ReturningTuple>(table_, sets_, where_,
                                                           std::move(returning_tuple));
  }
};

/// @brief Create an UPDATE query for the specified table
/// @tparam Table The table type
/// @param table The table to update
/// @return An UpdateQuery object
template <TableType Table>
auto update(const Table& table) {
  return UpdateQuery<Table>(table);
}

}  // namespace query
}  // namespace relx
