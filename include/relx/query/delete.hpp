#pragma once

#include "column_expression.hpp"
#include "condition.hpp"
#include "core.hpp"
#include "operators.hpp"
#include "value.hpp"

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace relx::query {

/// @brief Base DELETE query builder
/// @tparam Table Table to delete from
/// @tparam Where Optional where condition
/// @tparam ReturningColumns Tuple of column expressions to return after delete
template <TableType Table, typename Where = std::nullopt_t,
          typename ReturningColumns = std::tuple<>>
class DeleteQuery {
private:
  Table table_;
  Where where_;
  ReturningColumns returning_columns_;

  // Helper to convert the RETURNING clause to SQL
  constexpr std::string returning_to_sql() const {
    if constexpr (is_empty_tuple<ReturningColumns>()) {
      return "";
    } else {
      return " RETURNING " + tuple_to_sql(returning_columns_, ", ");
    }
  }

  // Helper to collect bind parameters from RETURNING clause
  constexpr std::vector<bind_param> returning_bind_params() const {
    std::vector<bind_param> params;

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
  using where_type = Where;
  using returning_columns_type = ReturningColumns;

  /// @brief Constructor for the DELETE query builder
  /// @param table The table to delete from
  /// @param where The WHERE condition
  /// @param returning_columns The columns to return after deletion
  constexpr explicit DeleteQuery(Table table, Where where = std::nullopt,
                                 ReturningColumns returning_columns = {})
      : table_(std::move(table)), where_(std::move(where)),
        returning_columns_(std::move(returning_columns)) {}

  /// @brief Generate the SQL for this DELETE query
  /// @return The SQL string
  constexpr std::string to_sql() const {
    std::string out = "DELETE FROM ";
    out += table_.table_name;

    // Add WHERE clause
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        out += " WHERE " + where_.value().to_sql();
      }
    }

    out += returning_to_sql();

    return out;
  }

  /// @brief Get the bind parameters for this DELETE query
  /// @return Vector of bind parameters
  constexpr std::vector<bind_param> bind_params() const {
    std::vector<bind_param> params;

    // Collect parameters from WHERE clause
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        auto where_params = where_.value().bind_params();
        params.insert(params.end(), where_params.begin(), where_params.end());
      }
    }

    auto returning_params = returning_bind_params();
    params.insert(params.end(), returning_params.begin(), returning_params.end());

    return params;
  }

  /// @brief Add a WHERE clause to the query
  /// @tparam Condition The condition type
  /// @param cond The WHERE condition
  /// @return New DeleteQuery with the WHERE clause added
  template <ConditionExpr Condition>
  constexpr auto where(const Condition& cond) const {
    return DeleteQuery<Table, std::optional<Condition>, ReturningColumns>(
        table_, std::optional<Condition>(cond), returning_columns_);
  }

  /// @brief Set a condition for filtering the rows to delete using IN with values
  /// @tparam Col The column type
  /// @tparam Range The range type for IN values
  /// @param column The column to check
  /// @param values The values to check against
  /// @return New DeleteQuery with the IN condition added
  template <ColumnType Col, std::ranges::range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
  auto where_in(const Col& column, const Range& values) const {
    auto col_expr = column_ref(column);
    auto in_condition = in(col_expr, values);
    return where(in_condition);
  }

  /// @brief Specify columns to return after delete
  /// @tparam Args Column types or SQL expressions
  /// @param args The columns or expressions to return
  /// @return New DeleteQuery with the RETURNING clause added
  template <typename... Args>
  constexpr auto returning(const Args&... args) const {
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

    return DeleteQuery<Table, Where, ReturningTuple>(table_, where_, std::move(returning_tuple));
  }

  /// @brief RETURNING every column of the table, expanded via reflection
  auto returning_all() const {
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
      return returning(table_.[:detail::select_all_columns<Table>()[I]:]...);
    }(std::make_index_sequence<detail::select_all_columns<Table>().size()>{});
  }
};

/// @brief Create a DELETE query for the specified table
/// @tparam Table The table type
/// @param table The table to delete from
/// @return A DeleteQuery object
template <TableType Table>
constexpr auto delete_from(const Table& table) {
  return DeleteQuery<Table>(table);
}

}  // namespace relx::query
