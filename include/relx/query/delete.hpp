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
template <TableType Table, typename Where = std::nullopt_t>
class DeleteQuery {
public:
  using table_type = Table;
  using where_type = Where;

  /// @brief Constructor for the DELETE query builder
  /// @param table The table to delete from
  /// @param where The WHERE condition
  explicit DeleteQuery(Table table, Where where = std::nullopt)
      : table_(std::move(table)), where_(std::move(where)) {}

  /// @brief Generate the SQL for this DELETE query
  /// @return The SQL string
  std::string to_sql() const {
    std::stringstream ss;
    ss << "DELETE FROM " << table_.table_name;

    // Add WHERE clause
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        ss << " WHERE " << where_.value().to_sql();
      }
    }

    return ss.str();
  }

  /// @brief Get the bind parameters for this DELETE query
  /// @return Vector of bind parameters
  std::vector<std::string> bind_params() const {
    std::vector<std::string> params;

    // Collect parameters from WHERE clause
    if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
      if (where_.has_value()) {
        auto where_params = where_.value().bind_params();
        params.insert(params.end(), where_params.begin(), where_params.end());
      }
    }

    return params;
  }

  /// @brief Add a WHERE clause to the query
  /// @tparam Condition The condition type
  /// @param cond The WHERE condition
  /// @return New DeleteQuery with the WHERE clause added
  template <ConditionExpr Condition>
  auto where(const Condition& cond) const {
    return DeleteQuery<Table, std::optional<Condition>>(table_, std::optional<Condition>(cond));
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

private:
  Table table_;
  Where where_;
};

/// @brief Create a DELETE query for the specified table
/// @tparam Table The table type
/// @param table The table to delete from
/// @return A DeleteQuery object
template <TableType Table>
auto delete_from(const Table& table) {
  return DeleteQuery<Table>(table);
}

}  // namespace relx::query