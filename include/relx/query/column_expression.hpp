#pragma once

#include "../schema/identifier.hpp"
#include "core.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace relx::query {

/// @brief Base class for column expressions
class ColumnExpression : public SqlExpression {
public:
  constexpr ~ColumnExpression() override = default;
  constexpr virtual std::string column_name() const = 0;
  constexpr virtual std::string table_name() const = 0;
  constexpr virtual std::string qualified_name() const {
    std::string qualified = schema::quote_identifier(column_name());
    auto table = table_name();
    if (!table.empty()) {
      qualified = schema::quote_identifier(table) + "." + qualified;
    }
    return qualified;
  }
};

/// @brief Column reference expression
/// @tparam Column The column type
template <ColumnType Column>
class ColumnRef : public ColumnExpression {
public:
  using column_type = Column;
  using value_type = typename Column::value_type;

  constexpr explicit ColumnRef(const Column& col) : col_(col) {}

  static_assert(std::is_empty_v<Column>, "columns are expected to be stateless");

  constexpr std::string to_sql() const override { return qualified_name(); }

  constexpr std::vector<bind_param> bind_params() const override { return {}; }

  constexpr std::string column_name() const override { return std::string(Column::name); }

  constexpr std::string table_name() const override {
    // Get the table name from the parent table class
    using parent_table = typename Column::table_type;
    return std::string(parent_table::table_name);
  }

  const Column& column() const { return col_; }

private:
  // Stored by value: column objects are stateless, and a reference would dangle
  // when the column comes from a temporary table object
  Column col_;
};

/// @brief Create a column reference expression
/// @tparam Column The column type
/// @param col The column
/// @return A ColumnRef expression
template <ColumnType Column>
constexpr auto column_ref(const Column& col) {
  return ColumnRef<Column>(col);
}

/// @brief Column with an alias
/// @tparam Expr The expression type
template <SqlExpr Expr>
class AliasedColumn : public ColumnExpression {
public:
  // By-value storage: expression objects are small, and a constexpr-constructible
  // member lets runtime-aliased columns constant-evaluate in static_sql contexts.
  constexpr AliasedColumn(Expr expr, std::string alias)
      : expr_(std::move(expr)), alias_(std::move(alias)) {}

  // The alias is quoted like every identifier: mixed case survives PostgreSQL's
  // folding (so by-name result matching works) and special characters cannot splice
  // into the SQL text
  constexpr std::string to_sql() const override {
    return expr_.to_sql() + " AS " + schema::quote_identifier(alias_);
  }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

  constexpr std::string column_name() const override { return alias_; }

  constexpr std::string table_name() const override { return ""; }

private:
  Expr expr_;
  std::string alias_;
};

/// @brief Create an aliased column expression
/// @tparam Expr The expression type
/// @param expr The expression to alias
/// @param alias The alias name
/// @return An AliasedColumn expression
template <SqlExpr Expr>
constexpr auto as(Expr expr, std::string alias) {
  return AliasedColumn<Expr>(std::move(expr), std::move(alias));
}

/// @brief Create an aliased column expression from a column reference
/// @tparam Column The column type
/// @param column The column reference
/// @param alias The alias name
/// @return An AliasedColumn expression
template <ColumnType Column>
constexpr auto as(const Column& column, std::string alias) {
  return AliasedColumn<ColumnRef<Column>>(column_ref(column), std::move(alias));
}

}  // namespace relx::query