#pragma once

#include "column_expression.hpp"
#include "core.hpp"
#include "meta.hpp"
#include "select.hpp"
#include "value.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace relx::query {

/// @brief Represents a single column-value pair for an INSERT statement
/// @tparam Column The column type
/// @tparam ValExpr The value expression type
template <ColumnType Column, SqlExpr ValExpr>
struct InsertItem {
  ColumnRef<Column> column;
  ValExpr value;

  // Constructor to ensure the InsertItem can be properly initialized
  InsertItem(ColumnRef<Column> col, ValExpr val) : column(std::move(col)), value(std::move(val)) {}

  std::string column_name() const { return column.column_name(); }

  std::string value_sql() const { return value.to_sql(); }

  std::vector<std::string> bind_params() const { return value.bind_params(); }
};

/// @brief Base INSERT query builder
/// @tparam Table Table to insert into
/// @tparam Columns Tuple of column references
/// @tparam Values Tuple of value tuples for multi-row inserts, or empty for other insertion types
/// @tparam SelectQuery Optional SELECT query for INSERT ... SELECT statements
/// @tparam ReturningColumns Tuple of column expressions to return after insertion
template <TableType Table, typename Columns = std::tuple<>, typename Values = std::tuple<>,
          typename SelectStmt = std::nullopt_t, typename ReturningColumns = std::tuple<>>
class InsertQuery {
private:
  Table table_;
  Columns columns_;
  Values values_;
  SelectStmt select_;
  ReturningColumns returning_columns_;

  // Helper to convert a tuple of column references to column names for INSERT
  std::string columns_to_sql() const {
    std::stringstream ss;
    ss << "(";
    int i = 0;
    std::apply(
        [&](const auto&... cols) { ((ss << (i++ > 0 ? ", " : "") << cols.column_name()), ...); },
        columns_);
    ss << ")";
    return ss.str();
  }

  // Helper to convert a tuple of values to SQL for a single row VALUES clause
  template <typename ValueTuple>
  std::string values_row_to_sql(const ValueTuple& value_tuple) const {
    std::stringstream ss;
    ss << "(";
    int i = 0;
    std::apply([&](const auto&... vals) { ((ss << (i++ > 0 ? ", " : "") << vals.to_sql()), ...); },
               value_tuple);
    ss << ")";
    return ss.str();
  }

  // Helper to convert a tuple of value tuples to SQL for the VALUES clause
  std::string values_to_sql() const {
    std::stringstream ss;
    ss << "VALUES ";
    int i = 0;
    std::apply(
        [&](const auto&... value_tuples) {
          ((ss << (i++ > 0 ? ", " : "") << values_row_to_sql(value_tuples)), ...);
        },
        values_);
    return ss.str();
  }

  // Helper to collect bind parameters from a tuple of values
  template <typename ValueTuple>
  std::vector<std::string> values_row_bind_params(const ValueTuple& value_tuple) const {
    std::vector<std::string> params;

    std::apply(
        [&](const auto&... vals) {
          auto process_val = [&params](const auto& val) {
            auto val_params = val.bind_params();
            params.insert(params.end(), val_params.begin(), val_params.end());
          };

          (process_val(vals), ...);
        },
        value_tuple);

    return params;
  }

  // Helper to collect bind parameters from a tuple of value tuples
  std::vector<std::string> values_bind_params() const {
    std::vector<std::string> params;

    std::apply(
        [&](const auto&... value_tuples) {
          auto process_tuple = [&params, this](const auto& tuple) {
            auto tuple_params = values_row_bind_params(tuple);
            params.insert(params.end(), tuple_params.begin(), tuple_params.end());
          };

          (process_tuple(value_tuples), ...);
        },
        values_);

    return params;
  }

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
  using columns_type = Columns;
  using values_type = Values;
  using select_type = SelectStmt;
  using returning_columns_type = ReturningColumns;

  /// @brief Constructor for the INSERT query builder
  /// @param table The table to insert into
  /// @param columns The columns to insert into
  /// @param values The values to insert
  /// @param select The SELECT statement (for INSERT ... SELECT)
  /// @param returning_columns The columns to return after insertion
  explicit InsertQuery(Table table, Columns columns = {}, Values values = {},
                       SelectStmt select = std::nullopt, ReturningColumns returning_columns = {})
      : table_(std::move(table)), columns_(std::move(columns)), values_(std::move(values)),
        select_(std::move(select)), returning_columns_(std::move(returning_columns)) {}

  /// @brief Generate the SQL for this INSERT query
  /// @return The SQL string
  std::string to_sql() const {
    std::stringstream ss;
    ss << "INSERT INTO " << table_.table_name;

    // Add columns clause if columns are specified
    if constexpr (!is_empty_tuple<Columns>()) {
      ss << " " << columns_to_sql();
    }

    // Handle different types of INSERT statements

    // INSERT ... VALUES ...
    if constexpr (!is_empty_tuple<Values>() && std::is_same_v<SelectStmt, std::nullopt_t>) {
      ss << " " << values_to_sql();
    }
    // INSERT ... SELECT ...
    else if constexpr (!std::is_same_v<SelectStmt, std::nullopt_t>) {
      if (select_.has_value()) {
        ss << " " << select_.value().to_sql();
      }
    }

    // Add RETURNING clause if specified
    ss << returning_to_sql();

    return ss.str();
  }

  /// @brief Get the bind parameters for this INSERT query
  /// @return Vector of bind parameters
  std::vector<std::string> bind_params() const {
    std::vector<std::string> params;

    // INSERT ... VALUES ...
    if constexpr (!is_empty_tuple<Values>() && std::is_same_v<SelectStmt, std::nullopt_t>) {
      auto values_params = values_bind_params();
      params.insert(params.end(), values_params.begin(), values_params.end());
    }
    // INSERT ... SELECT ...
    else if constexpr (!std::is_same_v<SelectStmt, std::nullopt_t>) {
      if (select_.has_value()) {
        auto select_params = select_.value().bind_params();
        params.insert(params.end(), select_params.begin(), select_params.end());
      }
    }

    // Add RETURNING bind parameters
    auto returning_params = returning_bind_params();
    params.insert(params.end(), returning_params.begin(), returning_params.end());

    return params;
  }

  /// @brief Specify columns to insert into
  /// @tparam Cols Column types
  /// @param cols The columns to insert into
  /// @return New InsertQuery with columns specified
  template <ColumnType... Cols>
  auto columns(const Cols&... cols) const {
    using NewColumns = std::tuple<ColumnRef<Cols>...>;
    auto column_refs = std::make_tuple(ColumnRef<Cols>(cols)...);

    return InsertQuery<Table, NewColumns, Values, SelectStmt, ReturningColumns>(
        table_, std::move(column_refs), values_, select_, returning_columns_);
  }

  /// @brief Add a row of values to insert
  /// @tparam Args The value expression types or raw value types
  /// @param args The values to insert (automatically wrapped with val() if not already SqlExpr)
  /// @return New InsertQuery with the values added
  template <typename... Args>
  auto values(Args&&... args) const {
    // Helper to convert arguments to SqlExpr if they're not already
    auto to_expr = [](auto&& arg) {
      if constexpr (SqlExpr<std::remove_cvref_t<decltype(arg)>>) {
        return std::forward<decltype(arg)>(arg);
      } else {
        return val(std::forward<decltype(arg)>(arg));
      }
    };

    using ValueTuple = std::tuple<decltype(to_expr(std::declval<Args>()))...>;
    auto value_tuple = std::make_tuple(to_expr(std::forward<Args>(args))...);

    // If we already have value tuples, add this one to them
    if constexpr (!is_empty_tuple<Values>()) {
      auto new_values = std::tuple_cat(values_, std::make_tuple(value_tuple));

      return InsertQuery<Table, Columns, decltype(new_values), SelectStmt, ReturningColumns>(
          table_, columns_, std::move(new_values), select_, returning_columns_);
    }
    // If this is the first value tuple, create a new tuple
    else {
      using NewValues = std::tuple<ValueTuple>;
      auto new_values = std::make_tuple(value_tuple);

      return InsertQuery<Table, Columns, NewValues, SelectStmt, ReturningColumns>(
          table_, columns_, std::move(new_values), select_, returning_columns_);
    }
  }

  /// @brief Set a SELECT query to use for INSERT ... SELECT statements
  /// @tparam Select The SELECT query type
  /// @param select The SELECT query
  /// @return New InsertQuery with the SELECT query set
  template <typename Select>
    requires SqlExpr<Select>
  auto select(const Select& select) const {
    return InsertQuery<Table, Columns, Values, std::optional<Select>, ReturningColumns>(
        table_, columns_, values_, std::optional<Select>(select), returning_columns_);
  }

  /// @brief Specify columns to return after insertion
  /// @tparam Args Column types or SQL expressions
  /// @param args The columns or expressions to return
  /// @return New InsertQuery with the RETURNING clause added
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

    return InsertQuery<Table, Columns, Values, SelectStmt, ReturningTuple>(
        table_, columns_, values_, select_, std::move(returning_tuple));
  }
};

/// @brief Create an INSERT query for the specified table
/// @tparam Table The table type
/// @param table The table to insert into
/// @return An InsertQuery object
template <TableType Table>
auto insert_into(const Table& table) {
  return InsertQuery<Table>(table);
}

}  // namespace relx::query