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

// Forward declarations
class CaseExpr;

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
  const Column& col_;
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
  AliasedColumn(const Expr& expr, std::string alias)
      : expr_(std::make_shared<Expr>(expr)), alias_(std::move(alias)) {}

  AliasedColumn(Expr&& expr, std::string alias)
      : expr_(std::make_shared<Expr>(std::move(expr))), alias_(std::move(alias)) {}

  // Special constructor for non-copyable types
  AliasedColumn(std::shared_ptr<Expr> expr, std::string alias)
      : expr_(std::move(expr)), alias_(std::move(alias)) {}

  constexpr std::string to_sql() const override { return expr_->to_sql() + " AS " + alias_; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_->bind_params(); }

  std::string column_name() const override { return alias_; }

  std::string table_name() const override { return ""; }

private:
  std::shared_ptr<Expr> expr_;
  std::string alias_;
};

/// @brief Create an aliased column expression
/// @tparam Expr The expression type
/// @param expr The expression to alias
/// @param alias The alias name
/// @return An AliasedColumn expression
template <SqlExpr Expr>
auto as(const Expr& expr, std::string alias) {
  return AliasedColumn<Expr>(expr, std::move(alias));
}

/// @brief Create an aliased column expression from a column reference
/// @tparam Column The column type
/// @param column The column reference
/// @param alias The alias name
/// @return An AliasedColumn expression
template <ColumnType Column>
auto as(const Column& column, std::string alias) {
  return AliasedColumn<ColumnRef<Column>>(column_ref(column), std::move(alias));
}

}  // namespace relx::query