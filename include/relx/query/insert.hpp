#pragma once

#include "../schema/identifier.hpp"
#include "column_expression.hpp"
#include "core.hpp"
#include "meta.hpp"
#include "select.hpp"
#include "value.hpp"
#include "write_meta.hpp"

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

  constexpr std::vector<bind_param> bind_params() const { return value.bind_params(); }
};

/// @brief Base INSERT query builder
/// @tparam Table Table to insert into
/// @tparam Columns Tuple of column references
/// @tparam Values Tuple of value tuples for multi-row inserts, or empty for other insertion types
/// @tparam SelectQuery Optional SELECT query for INSERT ... SELECT statements
/// @tparam ReturningColumns Tuple of column expressions to return after insertion
/// @tparam Upsert Whether to append an ON CONFLICT clause derived from the table's
/// primary key (see upsert())
template <TableType Table, typename Columns = std::tuple<>, typename Values = std::tuple<>,
          typename SelectStmt = std::nullopt_t, typename ReturningColumns = std::tuple<>,
          bool Upsert = false>
class InsertQuery {
private:
  Table table_;
  Columns columns_;
  Values values_;
  SelectStmt select_;
  ReturningColumns returning_columns_;

  // Helper to convert a tuple of column references to column names for INSERT
  constexpr std::string columns_to_sql() const {
    std::string out = "(";
    int i = 0;
    std::apply(
        [&](const auto&... cols) {
          ((out += (i++ > 0 ? ", " : ""), out += schema::quote_identifier(cols.column_name())),
           ...);
        },
        columns_);
    out += ")";
    return out;
  }

  // Helper to convert a tuple of values to SQL for a single row VALUES clause
  template <typename ValueTuple>
  constexpr std::string values_row_to_sql(const ValueTuple& value_tuple) const {
    return "(" + tuple_to_sql(value_tuple, ", ") + ")";
  }

  // Helper to convert a tuple of value tuples to SQL for the VALUES clause
  constexpr std::string values_to_sql() const {
    std::string out = "VALUES ";
    int i = 0;
    std::apply(
        [&](const auto&... value_tuples) {
          ((out += (i++ > 0 ? ", " : ""), out += values_row_to_sql(value_tuples)), ...);
        },
        values_);
    return out;
  }

  // Helper to collect bind parameters from a tuple of values
  template <typename ValueTuple>
  constexpr std::vector<bind_param> values_row_bind_params(const ValueTuple& value_tuple) const {
    std::vector<bind_param> params;

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
  constexpr std::vector<bind_param> values_bind_params() const {
    std::vector<bind_param> params;

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

  // Helper to build the ON CONFLICT clause for upsert queries. The conflict target is
  // the table's primary key; every other inserted column is overwritten from EXCLUDED.
  // When the insert list contains only key columns there is nothing to update, so the
  // clause degrades to DO NOTHING.
  constexpr std::string upsert_to_sql() const {
    constexpr auto pk_names = detail::pk_column_names<Table>();
    std::string out = " ON CONFLICT (";
    bool first = true;
    for (const std::string_view pk : pk_names) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += schema::quote_identifier(pk);
    }
    out += ")";

    std::string set_sql;
    std::apply(
        [&](const auto&... cols) {
          auto add = [&](const auto& col) {
            const std::string name = col.column_name();
            bool is_pk = false;
            for (const std::string_view pk : pk_names) {
              is_pk = is_pk || pk == name;
            }
            if (!is_pk) {
              if (!set_sql.empty()) {
                set_sql += ", ";
              }
              set_sql += schema::quote_identifier(name) + " = EXCLUDED." +
                         schema::quote_identifier(name);
            }
          };
          (add(cols), ...);
        },
        columns_);

    if (set_sql.empty()) {
      out += " DO NOTHING";
    } else {
      out += " DO UPDATE SET " + set_sql;
    }
    return out;
  }

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
  constexpr explicit InsertQuery(Table table, Columns columns = {}, Values values = {},
                                 SelectStmt select = std::nullopt,
                                 ReturningColumns returning_columns = {})
      : table_(std::move(table)), columns_(std::move(columns)), values_(std::move(values)),
        select_(std::move(select)), returning_columns_(std::move(returning_columns)) {}

  /// @brief Generate the SQL for this INSERT query
  /// @return The SQL string
  constexpr std::string to_sql() const {
    std::string out = "INSERT INTO ";
    out += schema::quote_identifier(std::string_view(table_.table_name));

    // Add columns clause if columns are specified
    if constexpr (!is_empty_tuple<Columns>()) {
      out += " " + columns_to_sql();
    }

    // Handle different types of INSERT statements

    // INSERT ... VALUES ...
    if constexpr (!is_empty_tuple<Values>() && std::is_same_v<SelectStmt, std::nullopt_t>) {
      out += " " + values_to_sql();
    }
    // INSERT ... SELECT ...
    else if constexpr (!std::is_same_v<SelectStmt, std::nullopt_t>) {
      if (select_.has_value()) {
        out += " " + select_.value().to_sql();
      }
    }

    if constexpr (Upsert) {
      out += upsert_to_sql();
    }

    // Add RETURNING clause if specified
    out += returning_to_sql();

    return out;
  }

  /// @brief Get the bind parameters for this INSERT query
  /// @return Vector of bind parameters
  constexpr std::vector<bind_param> bind_params() const {
    std::vector<bind_param> params;

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
  constexpr auto columns(const Cols&... cols) const {
    using NewColumns = std::tuple<ColumnRef<Cols>...>;
    auto column_refs = std::make_tuple(ColumnRef<Cols>(cols)...);

    return InsertQuery<Table, NewColumns, Values, SelectStmt, ReturningColumns, Upsert>(
        table_, std::move(column_refs), values_, select_, returning_columns_);
  }

  /// @brief Add a row of values to insert
  /// @tparam Args The value expression types or raw value types
  /// @param args The values to insert (automatically wrapped with val() if not already SqlExpr)
  /// @return New InsertQuery with the values added
  template <typename... Args>
  constexpr auto values(Args&&... args) const {
    // A row whose arity disagrees with the column list would bind values to the
    // wrong columns (or fail server-side after the fact)
    static_assert(std::tuple_size_v<Columns> == 0 || sizeof...(Args) == std::tuple_size_v<Columns>,
                  "insert values(): the number of values must match the number of "
                  "inserted columns");

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

      return InsertQuery<Table, Columns, decltype(new_values), SelectStmt, ReturningColumns,
                         Upsert>(table_, columns_, std::move(new_values), select_,
                                 returning_columns_);
    }
    // If this is the first value tuple, create a new tuple
    else {
      using NewValues = std::tuple<ValueTuple>;
      auto new_values = std::make_tuple(value_tuple);

      return InsertQuery<Table, Columns, NewValues, SelectStmt, ReturningColumns, Upsert>(
          table_, columns_, std::move(new_values), select_, returning_columns_);
    }
  }

  /// @brief Insert a row per object, expanding the table's columns via reflection.
  /// Every insertable column must have a same-named field on the object (a missing or
  /// type-incompatible field is a compile error naming the column). Auto-generated
  /// identity columns are skipped — the database assigns them. Disengaged
  /// std::optional fields bind a typed NULL parameter.
  ///
  /// ```cpp
  /// Users u{.id = 0 /* identity, skipped */, .username = "a", .email = "a@x"};
  /// auto q = insert_into(users).values_from(u).returning(users.id);
  /// ```
  template <typename Obj, typename... Rest>
    requires(!SqlExpr<Obj> && !ColumnType<Obj>)
  constexpr auto values_from(const Obj& obj, const Rest&... rest) const {
    static_assert(is_empty_tuple<Columns>() && is_empty_tuple<Values>(),
                  "values_from() derives the column list itself; call it on a fresh "
                  "insert_into(table) instead of combining it with columns()/values()");
    static_assert(detail::insertable_columns<Table>().size() > 0,
                  "the table has no insertable columns");
    static_assert(detail::values_from_diagnostics<Table, Obj>().empty(),
                  std::string("values_from(): object does not match the table's columns: ") +
                      detail::values_from_diagnostics<Table, Obj>());

    auto with_columns = [this]<std::size_t... I>(std::index_sequence<I...>) {
      return columns(table_.[:detail::insertable_columns<Table>()[I]:]...);
    }(std::make_index_sequence<detail::insertable_columns<Table>().size()>{});

    return add_rows_from(with_columns, obj, rest...);
  }

  /// @brief PostgreSQL upsert: appends ON CONFLICT on the table's primary key, updating
  /// every non-key inserted column from EXCLUDED (DO NOTHING when only key columns are
  /// inserted). The primary key comes from the pk annotation/modifier or composite_pk;
  /// it must be part of the inserted columns.
  ///
  /// ```cpp
  /// auto q = insert_into(users).values_from(u).upsert();
  /// // INSERT ... ON CONFLICT ("id") DO UPDATE SET "username" = EXCLUDED."username", ...
  /// ```
  constexpr auto upsert() const {
    static_assert(!is_empty_tuple<Columns>(),
                  "upsert() derives its SET list from the inserted columns; use it after "
                  "values_from() or columns()");
    static_assert(detail::pk_column_count<Table>() > 0,
                  "upsert() requires the table to declare a primary key");
    static_assert(detail::pk_covered_by_insert_columns<Table, Columns>(),
                  "upsert() requires every primary-key column to be part of the inserted "
                  "columns, otherwise the ON CONFLICT target can never be hit");
    return InsertQuery<Table, Columns, Values, SelectStmt, ReturningColumns, true>(
        table_, columns_, values_, select_, returning_columns_);
  }

  /// @brief Set a SELECT query to use for INSERT ... SELECT statements
  /// @tparam Select The SELECT query type
  /// @param select The SELECT query
  /// @return New InsertQuery with the SELECT query set
  template <typename Select>
    requires SqlExpr<Select>
  constexpr auto select(const Select& select) const {
    return InsertQuery<Table, Columns, Values, std::optional<Select>, ReturningColumns, Upsert>(
        table_, columns_, values_, std::optional<Select>(select), returning_columns_);
  }

  /// @brief Specify columns to return after insertion
  /// @tparam Args Column types or SQL expressions
  /// @param args The columns or expressions to return
  /// @return New InsertQuery with the RETURNING clause added
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

    return InsertQuery<Table, Columns, Values, SelectStmt, ReturningTuple, Upsert>(
        table_, columns_, values_, select_, std::move(returning_tuple));
  }

  /// @brief RETURNING every column of the table, expanded via reflection
  auto returning_all() const {
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
      return returning(table_.[:detail::select_all_columns<Table>()[I]:]...);
    }(std::make_index_sequence<detail::select_all_columns<Table>().size()>{});
  }

private:
  template <typename F>
  static constexpr auto wrap_field_value(const F& field) {
    return Value<F>(field);
  }

  // clang-format off

  /// Appends one VALUES row per object, mapping the table's insertable columns onto
  /// same-named object fields
  template <typename Query, typename Obj, typename... Rest>
  static constexpr auto add_rows_from(const Query& query, const Obj& obj, const Rest&... rest) {
    static_assert(detail::values_from_diagnostics<Table, Obj>().empty(),
                  std::string("values_from(): object does not match the table's columns: ") +
                      detail::values_from_diagnostics<Table, Obj>());
    auto with_row = [&]<std::size_t... I>(std::index_sequence<I...>) {
      return query.values(wrap_field_value(
          obj.[:detail::obj_field_for_column<Obj, detail::insertable_columns<Table>()[I]>():])...);
    }(std::make_index_sequence<detail::insertable_columns<Table>().size()>{});

    if constexpr (sizeof...(Rest) == 0) {
      return with_row;
    } else {
      return add_rows_from(with_row, rest...);
    }
  }

  // clang-format on
};

/// @brief Create an INSERT query for the specified table
/// @tparam Table The table type
/// @param table The table to insert into
/// @return An InsertQuery object
template <TableType Table>
constexpr auto insert_into(const Table& table) {
  return InsertQuery<Table>(table);
}

}  // namespace relx::query