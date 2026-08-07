#pragma once

#include "../schema/fixed_string.hpp"
#include "column_expression.hpp"
#include "core.hpp"
#include "schema_adapter.hpp"
#include "value.hpp"

#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace relx::query {

/// @brief Generic binary condition expression. The operator is part of the type, so a
/// query's type fully determines its SQL text (required for type-keyed SQL memoization
/// and prepared statements).
template <SqlExpr Left, SqlExpr Right, schema::fixed_string Op>
class BinaryCondition : public SqlExpression {
public:
  constexpr BinaryCondition(Left left, Right right)
      : left_(std::move(left)), right_(std::move(right)) {}

  constexpr std::string to_sql() const override {
    return "(" + left_.to_sql() + " " + std::string(std::string_view(Op)) + " " + right_.to_sql() +
           ")";
  }

  constexpr std::vector<bind_param> bind_params() const override {
    auto left_params = left_.bind_params();
    auto right_params = right_.bind_params();
    left_params.insert(left_params.end(), right_params.begin(), right_params.end());
    return left_params;
  }

private:
  Left left_;
  Right right_;
};

/// @brief Equality condition (col = value)
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator==(Left left, Right right) {
  return BinaryCondition<Left, Right, "=">(std::move(left), std::move(right));
}

/// @brief Inequality condition (col != value)
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator!=(Left left, Right right) {
  return BinaryCondition<Left, Right, "!=">(std::move(left), std::move(right));
}

/// @brief Greater than condition (col > value)
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator>(Left left, Right right) {
  return BinaryCondition<Left, Right, ">">(std::move(left), std::move(right));
}

/// @brief Less than condition (col < value)
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator<(Left left, Right right) {
  return BinaryCondition<Left, Right, "<">(std::move(left), std::move(right));
}

/// @brief Greater than or equal condition (col >= value)
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator>=(Left left, Right right) {
  return BinaryCondition<Left, Right, ">=">(std::move(left), std::move(right));
}

/// @brief Less than or equal condition (col <= value)
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator<=(Left left, Right right) {
  return BinaryCondition<Left, Right, "<=">(std::move(left), std::move(right));
}

/// @brief Logical AND condition
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator&&(Left left, Right right) {
  return BinaryCondition<Left, Right, "AND">(std::move(left), std::move(right));
}

/// @brief Logical OR condition
template <SqlExpr Left, SqlExpr Right>
constexpr auto operator||(Left left, Right right) {
  return BinaryCondition<Left, Right, "OR">(std::move(left), std::move(right));
}

/// @brief Range elements usable in an IN list: strings bind as text directly,
/// anything else goes through the same typed conversion as relx::val()
template <typename U>
concept InListElement = std::convertible_to<U, std::string> || requires(const U& u) {
  schema::column_traits<std::remove_cvref_t<U>>::to_sql_string(u);
};

namespace detail {

/// @brief One IN-list element as a bind parameter (typed for non-string elements)
template <typename U>
bind_param in_list_param(const U& value) {
  if constexpr (std::convertible_to<U, std::string>) {
    return bind_param(std::string(value));
  } else {
    return Value<std::remove_cvref_t<U>>(value).bind_params().front();
  }
}

}  // namespace detail

/// @brief IN condition (col IN (values)) with type checking
template <SqlExpr Expr, std::ranges::range Range>
  requires InListElement<std::ranges::range_value_t<Range>>
class TypedInCondition : public SqlExpression {
public:
  constexpr TypedInCondition(Expr expr, Range values)
      : expr_(std::move(expr)), values_(std::move(values)) {}

  constexpr std::string to_sql() const override {
    // An empty list would render "IN ()" - a PostgreSQL syntax error. Nothing is a
    // member of the empty set, so the condition is constant false (bind_params
    // matches by contributing nothing).
    if (std::ranges::empty(values_)) {
      return "1 = 0";
    }
    std::string out = expr_.to_sql() + " IN (";
    bool first = true;
    for (const auto& _ : values_) {
      if (!first) {
        out += ", ";
      }
      out += "?";
      first = false;
    }
    out += ")";
    return out;
  }

  constexpr std::vector<bind_param> bind_params() const override {
    if (std::ranges::empty(values_)) {
      return {};  // to_sql renders the constant "1 = 0" with no placeholders
    }
    auto params = expr_.bind_params();
    for (const auto& value : values_) {
      params.push_back(detail::in_list_param(value));
    }
    return params;
  }

private:
  Expr expr_;
  Range values_;
};

/// @brief Create an IN condition with type checking for columns
/// @tparam Column The column type
/// @tparam Range The values range type
/// @param col The column
/// @param values The values to check against
/// @return An InCondition expression
template <typename TableT, schema::fixed_string Name, typename T, typename... Modifiers,
          std::ranges::range Range>
  requires InListElement<std::ranges::range_value_t<Range>>
constexpr auto in(const schema::column<TableT, Name, T, Modifiers...>& col, Range values) {
  using ValueType = std::ranges::range_value_t<Range>;

  // The values must be compatible with the column type (or be pre-rendered strings)
  static_assert(std::convertible_to<ValueType, T> || std::convertible_to<ValueType, std::string>,
                "IN list values must be convertible to the column's value type");

  auto col_expr = to_expr(col);
  return TypedInCondition<decltype(col_expr), Range>(std::move(col_expr), std::move(values));
}

/// @brief Original IN condition for backward compatibility
template <SqlExpr Expr, std::ranges::range Range>
  requires InListElement<std::ranges::range_value_t<Range>>
class InCondition : public SqlExpression {
public:
  constexpr InCondition(Expr expr, Range values)
      : expr_(std::move(expr)), values_(std::move(values)) {}

  constexpr std::string to_sql() const override {
    // An empty list would render "IN ()" - a PostgreSQL syntax error. Nothing is a
    // member of the empty set, so the condition is constant false (bind_params
    // matches by contributing nothing).
    if (std::ranges::empty(values_)) {
      return "1 = 0";
    }
    std::string out = expr_.to_sql() + " IN (";
    bool first = true;
    for (const auto& _ : values_) {
      if (!first) {
        out += ", ";
      }
      out += "?";
      first = false;
    }
    out += ")";
    return out;
  }

  constexpr std::vector<bind_param> bind_params() const override {
    if (std::ranges::empty(values_)) {
      return {};  // to_sql renders the constant "1 = 0" with no placeholders
    }
    auto params = expr_.bind_params();
    for (const auto& value : values_) {
      params.push_back(detail::in_list_param(value));
    }
    return params;
  }

private:
  Expr expr_;
  Range values_;
};

/// @brief Create an IN condition for expressions
/// @tparam Expr The expression type
/// @tparam Range The values range type
/// @param expr The column or expression
/// @param values The values to check against
/// @return An InCondition expression
template <SqlExpr Expr, std::ranges::range Range>
  requires InListElement<std::ranges::range_value_t<Range>>
constexpr auto in(Expr expr, Range values) {
  return InCondition<Expr, Range>(std::move(expr), std::move(values));
}

/// @brief A complete SELECT query usable as a subquery in a condition
template <typename T>
concept SelectQueryExpr = SqlExpr<T> && requires { typename T::is_select_query; };

/// @brief IN-subquery condition (col IN (SELECT ...)). The subquery's SQL text is
/// inlined in parentheses; its bind parameters follow the outer expression's, in
/// order. Correlated subqueries work by referencing the outer table's columns in
/// the inner query's conditions.
template <SqlExpr Expr, SelectQueryExpr Query>
class InSubqueryCondition : public SqlExpression {
public:
  constexpr InSubqueryCondition(Expr expr, Query query)
      : expr_(std::move(expr)), query_(std::move(query)) {}

  constexpr std::string to_sql() const override {
    return expr_.to_sql() + " IN (" + query_.to_sql() + ")";
  }

  constexpr std::vector<bind_param> bind_params() const override {
    auto params = expr_.bind_params();
    auto sub_params = query_.bind_params();
    params.insert(params.end(), sub_params.begin(), sub_params.end());
    return params;
  }

private:
  Expr expr_;
  Query query_;
};

/// @brief Create an IN-subquery condition for a column: col IN (SELECT ...)
template <typename TableT, schema::fixed_string Name, typename T, typename... Modifiers,
          SelectQueryExpr Query>
constexpr auto in(const schema::column<TableT, Name, T, Modifiers...>& col, Query query) {
  auto col_expr = to_expr(col);
  return InSubqueryCondition<decltype(col_expr), Query>(std::move(col_expr), std::move(query));
}

/// @brief Create an IN-subquery condition for an arbitrary expression
template <SqlExpr Expr, SelectQueryExpr Query>
constexpr auto in(Expr expr, Query query) {
  return InSubqueryCondition<Expr, Query>(std::move(expr), std::move(query));
}

/// @brief EXISTS / NOT EXISTS condition over a subquery
template <SelectQueryExpr Query, bool Negated>
class ExistsCondition : public SqlExpression {
public:
  constexpr explicit ExistsCondition(Query query) : query_(std::move(query)) {}

  constexpr std::string to_sql() const override {
    return (Negated ? std::string("NOT EXISTS (") : std::string("EXISTS (")) + query_.to_sql() +
           ")";
  }

  constexpr std::vector<bind_param> bind_params() const override { return query_.bind_params(); }

private:
  Query query_;
};

/// @brief EXISTS (SELECT ...) condition. Correlate by referencing the outer table's
/// columns inside the subquery's where().
template <SelectQueryExpr Query>
constexpr auto exists(Query query) {
  return ExistsCondition<Query, false>(std::move(query));
}

/// @brief NOT EXISTS (SELECT ...) condition
template <SelectQueryExpr Query>
constexpr auto not_exists(Query query) {
  return ExistsCondition<Query, true>(std::move(query));
}

/// @brief ANY condition (col = ANY(?)) - the whole list travels as ONE array
/// parameter, so unlike IN the SQL text is independent of the list size: the query is
/// static-shaped (memoizable, preparable) and an empty list is valid (matches no
/// rows, where IN () would be a SQL syntax error).
template <SqlExpr Expr, relx::ArrayBindElement T>
class AnyCondition : public SqlExpression {
public:
  constexpr AnyCondition(Expr expr, std::vector<T> values)
      : expr_(std::move(expr)), values_(std::move(values)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " = ANY(?)"; }

  constexpr std::vector<bind_param> bind_params() const override {
    auto params = expr_.bind_params();
    params.push_back(relx::make_array_bind_param(values_));
    return params;
  }

private:
  Expr expr_;
  std::vector<T> values_;
};

/// @brief Create an ANY condition for a column: col = ANY of the given values
template <typename TableT, schema::fixed_string Name, typename T, typename... Modifiers,
          relx::ArrayBindElement U>
constexpr auto in_any(const schema::column<TableT, Name, T, Modifiers...>& col,
                      std::vector<U> values) {
  auto col_expr = to_expr(col);
  return AnyCondition<decltype(col_expr), U>(std::move(col_expr), std::move(values));
}

/// @brief Create an ANY condition for an arbitrary expression
template <SqlExpr Expr, relx::ArrayBindElement U>
constexpr auto in_any(Expr expr, std::vector<U> values) {
  return AnyCondition<Expr, U>(std::move(expr), std::move(values));
}

/// @brief LIKE condition (col LIKE pattern)
template <SqlExpr Expr>
class LikeCondition : public SqlExpression {
public:
  constexpr LikeCondition(Expr expr, std::string pattern)
      : expr_(std::move(expr)), pattern_(std::move(pattern)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " LIKE ?"; }

  constexpr std::vector<bind_param> bind_params() const override {
    auto params = expr_.bind_params();
    params.push_back(pattern_);
    return params;
  }

private:
  Expr expr_;
  std::string pattern_;
};

/// @brief Create a LIKE condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @param pattern The LIKE pattern
/// @return A LikeCondition expression
template <SqlExpr Expr>
constexpr auto like(Expr expr, std::string pattern) {
  return LikeCondition<Expr>(std::move(expr), std::move(pattern));
}

/// @brief BETWEEN condition (col BETWEEN lower AND upper)
template <SqlExpr Expr>
class BetweenCondition : public SqlExpression {
public:
  constexpr BetweenCondition(Expr expr, std::string lower, std::string upper)
      : expr_(std::move(expr)), lower_(std::move(lower)), upper_(std::move(upper)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " BETWEEN ? AND ?"; }

  constexpr std::vector<bind_param> bind_params() const override {
    auto params = expr_.bind_params();
    params.push_back(lower_);
    params.push_back(upper_);
    return params;
  }

private:
  Expr expr_;
  std::string lower_;
  std::string upper_;
};

/// @brief Create a BETWEEN condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @param lower The lower bound
/// @param upper The upper bound
/// @return A BetweenCondition expression
template <SqlExpr Expr>
constexpr auto between(Expr expr, std::string lower, std::string upper) {
  return BetweenCondition<Expr>(std::move(expr), std::move(lower), std::move(upper));
}

/// @brief IS NULL condition
template <SqlExpr Expr>
class IsNullCondition : public SqlExpression {
public:
  explicit IsNullCondition(Expr expr) : expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " IS NULL"; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

private:
  Expr expr_;
};

/// @brief Create an IS NULL condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @return An IsNullCondition expression
template <SqlExpr Expr>
constexpr auto is_null(Expr expr) {
  return IsNullCondition<Expr>(std::move(expr));
}

/// @brief IS NOT NULL condition
template <SqlExpr Expr>
class IsNotNullCondition : public SqlExpression {
public:
  explicit IsNotNullCondition(Expr expr) : expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override { return expr_.to_sql() + " IS NOT NULL"; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

private:
  Expr expr_;
};

/// @brief Create an IS NOT NULL condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @return An IsNotNullCondition expression
template <SqlExpr Expr>
constexpr auto is_not_null(Expr expr) {
  return IsNotNullCondition<Expr>(std::move(expr));
}

/// @brief Negation condition (NOT expr)
template <SqlExpr Expr>
class NotCondition : public SqlExpression {
public:
  explicit NotCondition(Expr expr) : expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override { return "(NOT " + expr_.to_sql() + ")"; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

private:
  Expr expr_;
};

/// @brief Logical NOT operator
template <SqlExpr Expr>
auto operator!(Expr expr) {
  return NotCondition<Expr>(std::move(expr));
}

}  // namespace relx::query