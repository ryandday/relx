#pragma once

#include "column_expression.hpp"
#include "core.hpp"
#include "value.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace relx {
namespace query {

/// @brief Base class for SQL function expressions
/// @tparam Expr The expression type for the function argument
template <SqlExpr Expr>
class FunctionExpr : public ColumnExpression {
public:
  FunctionExpr(std::string name, Expr expr) : func_name_(std::move(name)), expr_(std::move(expr)) {}

  std::string to_sql() const override { return func_name_ + "(" + expr_.to_sql() + ")"; }

  std::vector<std::string> bind_params() const override { return expr_.bind_params(); }

  std::string column_name() const override { return func_name_ + "(" + expr_.column_name() + ")"; }

  std::string table_name() const override {
    if constexpr (std::is_base_of_v<ColumnExpression, Expr>) {
      // Propagate table name from the expression if it's a column expression
      return expr_.table_name();
    } else {
      return "";
    }
  }

private:
  std::string func_name_;
  Expr expr_;
};

/// @brief Base class for SQL function expressions with no arguments
class NullaryFunctionExpr : public ColumnExpression {
public:
  explicit NullaryFunctionExpr(std::string name) : func_name_(std::move(name)) {}

  std::string to_sql() const override { return func_name_ + "()"; }

  std::vector<std::string> bind_params() const override { return {}; }

  std::string column_name() const override { return func_name_ + "()"; }

  std::string table_name() const override { return ""; }

private:
  std::string func_name_;
};

/// @brief Expression representing COUNT(*) in SQL
class CountAllExpr : public ColumnExpression {
public:
  std::string to_sql() const override { return "COUNT(*)"; }

  std::vector<std::string> bind_params() const override { return {}; }

  std::string column_name() const override { return "COUNT(*)"; }

  std::string table_name() const override { return ""; }
};

/// @brief COUNT(*) aggregate function
/// @return A NullaryFunctionExpr representing COUNT(*)
inline auto count_all() {
  return CountAllExpr{};
}

/// @brief COUNT aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to count
/// @return A FunctionExpr representing COUNT(expr)
template <SqlExpr Expr>
auto count(Expr expr) {
  return FunctionExpr<Expr>("COUNT", std::move(expr));
}

// Overload for column types
template <typename T>
  requires ColumnType<T>
auto count(const T& column) {
  return count(to_expr(column));
}

/// @brief COUNT(DISTINCT expr) aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to count distinct values of
/// @return A FunctionExpr representing COUNT(DISTINCT expr)
template <SqlExpr Expr>
auto count_distinct(Expr expr) {
  return count(distinct(std::move(expr)));
}

// Overload for column types
template <typename T>
  requires ColumnType<T>
auto count_distinct(const T& column) {
  return count_distinct(to_expr(column));
}

/// @brief Type checking concepts for aggregate functions
namespace aggregate_checking {

/// @brief Concept for numeric types suitable for SUM and AVG
template <typename T>
concept Summable = std::is_arithmetic_v<std::remove_cvref_t<T>> &&
                   !std::same_as<std::remove_cvref_t<T>, bool>;

/// @brief Concept for types suitable for COUNT (any type is countable)
template <typename T>
concept Countable = true;

/// @brief Concept for comparable types suitable for MIN/MAX
template <typename T>
concept Comparable = std::is_arithmetic_v<std::remove_cvref_t<T>> ||
                     std::same_as<std::remove_cvref_t<T>, std::string> ||
                     std::same_as<std::remove_cvref_t<T>, std::string_view>;

/// @brief Helper to extract column type for checking
template <typename T>
struct extract_column_type {
  using type = T;
};

template <typename TableT, schema::FixedString Name, typename ColumnT, typename... Modifiers>
struct extract_column_type<schema::column<TableT, Name, ColumnT, Modifiers...>> {
  using type = ColumnT;
};

template <typename T>
using extract_column_type_t = typename extract_column_type<T>::type;
}  // namespace aggregate_checking

/// @brief SUM aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to sum
/// @return A FunctionExpr representing SUM(expr)
template <SqlExpr Expr>
auto sum(Expr expr) {
  return FunctionExpr<Expr>("SUM", std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires ColumnType<T>
auto sum(const T& column) {
  using column_type = aggregate_checking::extract_column_type_t<T>;
  static_assert(aggregate_checking::Summable<column_type>,
                "SUM can only be used with numeric columns (int, long, float, double, etc.). "
                "Boolean columns are not summable.");
  return sum(to_expr(column));
}

/// @brief AVG aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to average
/// @return A FunctionExpr representing AVG(expr)
template <SqlExpr Expr>
auto avg(Expr expr) {
  return FunctionExpr<Expr>("AVG", std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires ColumnType<T>
auto avg(const T& column) {
  using column_type = aggregate_checking::extract_column_type_t<T>;
  static_assert(aggregate_checking::Summable<column_type>,
                "AVG can only be used with numeric columns (int, long, float, double, etc.). "
                "Boolean and string columns cannot be averaged.");
  return avg(to_expr(column));
}

/// @brief MIN aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to find minimum of
/// @return A FunctionExpr representing MIN(expr)
template <SqlExpr Expr>
auto min(Expr expr) {
  return FunctionExpr<Expr>("MIN", std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires ColumnType<T>
auto min(const T& column) {
  using column_type = aggregate_checking::extract_column_type_t<T>;
  static_assert(aggregate_checking::Comparable<column_type>,
                "MIN can only be used with comparable columns (numeric types or strings). "
                "Boolean columns cannot be compared for MIN/MAX.");
  return min(to_expr(column));
}

/// @brief MAX aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to find maximum of
/// @return A FunctionExpr representing MAX(expr)
template <SqlExpr Expr>
auto max(Expr expr) {
  return FunctionExpr<Expr>("MAX", std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires ColumnType<T>
auto max(const T& column) {
  using column_type = aggregate_checking::extract_column_type_t<T>;
  static_assert(aggregate_checking::Comparable<column_type>,
                "MAX can only be used with comparable columns (numeric types or strings). "
                "Boolean columns cannot be compared for MIN/MAX.");
  return max(to_expr(column));
}

/// @brief DISTINCT qualifier for an expression
/// @tparam Expr The expression type
/// @param expr The expression to apply DISTINCT to
/// @return A SqlExpression representing DISTINCT expr
template <SqlExpr Expr>
class DistinctExpr : public ColumnExpression {
public:
  explicit DistinctExpr(Expr expr) : expr_(std::move(expr)) {}

  std::string to_sql() const override { return "DISTINCT " + expr_.to_sql(); }

  std::vector<std::string> bind_params() const override { return expr_.bind_params(); }

  std::string column_name() const override {
    if constexpr (std::is_base_of_v<ColumnExpression, Expr>) {
      return "DISTINCT_" + expr_.column_name();
    } else {
      return "DISTINCT_EXPR";
    }
  }

  std::string table_name() const override {
    if constexpr (std::is_base_of_v<ColumnExpression, Expr>) {
      return expr_.table_name();
    } else {
      return "";
    }
  }

private:
  Expr expr_;
};

/// @brief Create a DISTINCT expression
/// @tparam Expr The expression type
/// @param expr The expression to apply DISTINCT to
/// @return A DistinctExpr
template <SqlExpr Expr>
auto distinct(Expr expr) {
  return DistinctExpr<Expr>(std::move(expr));
}

// Overload for column types
template <typename T>
  requires ColumnType<T>
auto distinct(const T& column) {
  return distinct(to_expr(column));
}

/// @brief LOWER string function
/// @tparam Expr The expression type
/// @param expr The string expression to convert to lowercase
/// @return A FunctionExpr representing LOWER(expr)
template <SqlExpr Expr>
auto lower(Expr expr) {
  return FunctionExpr<Expr>("LOWER", std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires ColumnType<T>
auto lower(const T& column) {
  using column_type = aggregate_checking::extract_column_type_t<T>;
  static_assert(
      std::same_as<column_type, std::string> || std::same_as<column_type, std::string_view> ||
          std::same_as<column_type, const char*>,
      "LOWER can only be used with string columns (std::string, std::string_view, const char*). "
      "Numeric and boolean columns cannot be converted to lowercase.");
  return lower(to_expr(column));
}

/// @brief UPPER string function
/// @tparam Expr The expression type
/// @param expr The string expression to convert to uppercase
/// @return A FunctionExpr representing UPPER(expr)
template <SqlExpr Expr>
auto upper(Expr expr) {
  return FunctionExpr<Expr>("UPPER", std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires ColumnType<T>
auto upper(const T& column) {
  using column_type = aggregate_checking::extract_column_type_t<T>;
  static_assert(
      std::same_as<column_type, std::string> || std::same_as<column_type, std::string_view> ||
          std::same_as<column_type, const char*>,
      "UPPER can only be used with string columns (std::string, std::string_view, const char*). "
      "Numeric and boolean columns cannot be converted to uppercase.");
  return upper(to_expr(column));
}

/// @brief LENGTH string function
/// @tparam Expr The expression type
/// @param expr The string expression to get length of
/// @return A FunctionExpr representing LENGTH(expr)
template <SqlExpr Expr>
auto length(Expr expr) {
  return FunctionExpr<Expr>("LENGTH", std::move(expr));
}

// Overload for column types
template <typename T>
  requires ColumnType<T>
auto length(const T& column) {
  return length(to_expr(column));
}

/// @brief TRIM string function
/// @tparam Expr The expression type
/// @param expr The string expression to trim
/// @return A FunctionExpr representing TRIM(expr)
template <SqlExpr Expr>
auto trim(Expr expr) {
  return FunctionExpr<Expr>("TRIM", std::move(expr));
}

// Overload for column types
template <typename T>
  requires ColumnType<T>
auto trim(const T& column) {
  return trim(to_expr(column));
}

/// @brief COALESCE function
/// @tparam First The first expression type
/// @tparam Second The second expression type
/// @tparam Rest The types of the remaining expressions
/// @param first The first expression
/// @param second The second expression
/// @param rest The remaining expressions
/// @return A SqlExpression representing COALESCE(first, second, ...)
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
class CoalesceExpr : public ColumnExpression {
public:
  CoalesceExpr(First first, Second second, Rest... rest)
      : first_(std::move(first)), second_(std::move(second)),
        rest_(std::make_tuple(std::move(rest)...)) {}

  std::string to_sql() const override {
    std::stringstream ss;
    ss << "COALESCE(" << first_.to_sql() << ", " << second_.to_sql();

    std::apply([&](const auto&... items) { ((ss << ", " << items.to_sql()), ...); }, rest_);

    ss << ")";
    return ss.str();
  }

  std::vector<std::string> bind_params() const override {
    std::vector<std::string> params;

    auto first_params = first_.bind_params();
    params.insert(params.end(), first_params.begin(), first_params.end());

    auto second_params = second_.bind_params();
    params.insert(params.end(), second_params.begin(), second_params.end());

    std::apply(
        [&](const auto&... items) {
          ((params.insert(params.end(), items.bind_params().begin(), items.bind_params().end())),
           ...);
        },
        rest_);

    return params;
  }

  std::string column_name() const override { return "COALESCE"; }

  std::string table_name() const override { return ""; }

private:
  First first_;
  Second second_;
  std::tuple<Rest...> rest_;
};

/// @brief Create a COALESCE expression
/// @tparam First The first expression type
/// @tparam Second The second expression type
/// @tparam Rest The types of the remaining expressions
/// @param first The first expression
/// @param second The second expression
/// @param rest The remaining expressions
/// @return A CoalesceExpr
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto coalesce(First first, Second second, Rest... rest) {
  return CoalesceExpr<First, Second, Rest...>(std::move(first), std::move(second),
                                              std::move(rest)...);
}

// Overload for first argument as column type
template <typename T, SqlExpr Second, SqlExpr... Rest>
  requires ColumnType<T>
auto coalesce(const T& column, Second second, Rest... rest) {
  return coalesce(to_expr(column), std::move(second), std::move(rest)...);
}

// Overload for first and second arguments as column types
template <typename T1, typename T2, SqlExpr... Rest>
  requires ColumnType<T1> && ColumnType<T2>
auto coalesce(const T1& column1, const T2& column2, Rest... rest) {
  return coalesce(to_expr(column1), to_expr(column2), std::move(rest)...);
}

// Overload for column and string literal
template <typename T>
  requires ColumnType<T>
auto coalesce(const T& column, const char* str) {
  return coalesce(to_expr(column), val(str));
}

// Overload for column and std::string
template <typename T>
  requires ColumnType<T>
auto coalesce(const T& column, const std::string& str) {
  return coalesce(to_expr(column), val(str));
}

// Overload for column, column, and string literal
template <typename T1, typename T2>
  requires ColumnType<T1> && ColumnType<T2>
auto coalesce(const T1& column1, const T2& column2, const char* str) {
  return coalesce(to_expr(column1), to_expr(column2), val(str));
}

// First define CaseExpr class fully
class CaseExpr : public ColumnExpression {
public:
  using WhenThenPair = std::pair<std::unique_ptr<SqlExpression>, std::unique_ptr<SqlExpression>>;

  explicit CaseExpr(std::vector<WhenThenPair>&& when_thens,
                    std::unique_ptr<SqlExpression> else_expr)
      : when_thens_(std::move(when_thens)), else_expr_(std::move(else_expr)) {}

  // Allow move operations
  CaseExpr(CaseExpr&&) = default;
  CaseExpr& operator=(CaseExpr&&) = default;

  // Disable copy operations (due to unique_ptr)
  CaseExpr(const CaseExpr&) = delete;
  CaseExpr& operator=(const CaseExpr&) = delete;

  std::string to_sql() const override {
    std::string sql = "CASE";

    for (const auto& [when_cond, then_val] : when_thens_) {
      sql += " WHEN (";
      std::string cond = when_cond->to_sql();
      // Remove any existing outer parentheses to avoid double parentheses
      if (cond.size() >= 2 && cond.front() == '(' && cond.back() == ')') {
        cond = cond.substr(1, cond.size() - 2);
      }
      sql += cond + ") THEN " + then_val->to_sql();
    }

    if (else_expr_) {
      sql += " ELSE " + else_expr_->to_sql();
    }

    sql += " END";
    return sql;
  }

  std::vector<std::string> bind_params() const override {
    std::vector<std::string> params;

    // Interleave condition and value parameters in the expected order
    for (const auto& [when_cond, then_val] : when_thens_) {
      // First condition parameters
      auto when_params = when_cond->bind_params();
      params.insert(params.end(), when_params.begin(), when_params.end());

      // Then the value parameters
      auto then_params = then_val->bind_params();
      params.insert(params.end(), then_params.begin(), then_params.end());
    }

    // Finally collect ELSE parameters if present
    if (else_expr_) {
      auto else_params = else_expr_->bind_params();
      params.insert(params.end(), else_params.begin(), else_params.end());
    }

    return params;
  }

  std::string column_name() const override { return "CASE"; }

  std::string table_name() const override { return ""; }

private:
  std::vector<WhenThenPair> when_thens_;
  std::unique_ptr<SqlExpression> else_expr_;
};

// Then define CaseBuilder that uses CaseExpr with type checking
template <typename ResultType = void>
class TypedCaseBuilder {
public:
  using WhenThenPair = std::pair<std::unique_ptr<SqlExpression>, std::unique_ptr<SqlExpression>>;

private:
  std::vector<WhenThenPair> when_thens_;
  std::unique_ptr<SqlExpression> else_expr_;

public:
  TypedCaseBuilder() : when_thens_(), else_expr_(nullptr) {}

  // Move constructor
  TypedCaseBuilder(TypedCaseBuilder&& other) noexcept
      : when_thens_(std::move(other.when_thens_)), else_expr_(std::move(other.else_expr_)) {}

  // Move assignment
  TypedCaseBuilder& operator=(TypedCaseBuilder&& other) noexcept {
    if (this != &other) {
      when_thens_ = std::move(other.when_thens_);
      else_expr_ = std::move(other.else_expr_);
    }
    return *this;
  }

  // Delete copy constructor and assignment to prevent accidental copies
  TypedCaseBuilder(const TypedCaseBuilder&) = delete;
  TypedCaseBuilder& operator=(const TypedCaseBuilder&) = delete;

  // Template constructor for converting between different result types
  template <typename OtherType>
  explicit TypedCaseBuilder(TypedCaseBuilder<OtherType>&& other)
      : when_thens_(std::move(other.when_thens_)), else_expr_(std::move(other.else_expr_)) {}

  template <SqlExpr Then>
  auto when(const ConditionExpr auto& when_cond, const Then& then) {
    if constexpr (std::same_as<ResultType, void>) {
      // First WHEN clause - establish the result type
      when_thens_.emplace_back(
          std::make_unique<std::remove_cvref_t<decltype(when_cond)>>(when_cond),
          std::make_unique<Then>(then));
      return TypedCaseBuilder<Then>(std::move(*this));
    } else {
      // Subsequent WHEN clauses - ensure type compatibility
      static_assert(std::same_as<std::remove_cvref_t<Then>, std::remove_cvref_t<ResultType>>,
                    "All WHEN/THEN branches in CASE expressions must return the same type. "
                    "Mixed types in CASE branches can lead to runtime type errors.");

      when_thens_.emplace_back(
          std::make_unique<std::remove_cvref_t<decltype(when_cond)>>(when_cond),
          std::make_unique<Then>(then));
      return std::move(*this);
    }
  }

  // Specific overloads for common literal types to avoid template conflicts
  auto when(const ConditionExpr auto& when_cond, const char* then) {
    return this->when(when_cond, query::val(then));
  }

  auto when(const ConditionExpr auto& when_cond, const std::string& then) {
    return this->when(when_cond, query::val(then));
  }

  auto when(const ConditionExpr auto& when_cond, int then) {
    return this->when(when_cond, query::val(then));
  }

  auto when(const ConditionExpr auto& when_cond, long then) {
    return this->when(when_cond, query::val(then));
  }

  auto when(const ConditionExpr auto& when_cond, double then) {
    return this->when(when_cond, query::val(then));
  }

  auto when(const ConditionExpr auto& when_cond, float then) {
    return this->when(when_cond, query::val(then));
  }

  auto when(const ConditionExpr auto& when_cond, bool then) {
    return this->when(when_cond, query::val(then));
  }

  template <SqlExpr Else>
  auto else_(const Else& else_expr) {
    if constexpr (!std::same_as<ResultType, void>) {
      static_assert(std::same_as<std::remove_cvref_t<Else>, std::remove_cvref_t<ResultType>>,
                    "ELSE clause type must match the WHEN/THEN branch types in CASE expressions.");
    }

    else_expr_ = std::make_unique<Else>(else_expr);
    return std::move(*this);
  }

  // Specific overloads for common literal types in else clause
  auto else_(const char* else_value) { return this->else_(query::val(else_value)); }

  auto else_(const std::string& else_value) { return this->else_(query::val(else_value)); }

  auto else_(int else_value) { return this->else_(query::val(else_value)); }

  auto else_(long else_value) { return this->else_(query::val(else_value)); }

  auto else_(double else_value) { return this->else_(query::val(else_value)); }

  auto else_(float else_value) { return this->else_(query::val(else_value)); }

  auto else_(bool else_value) { return this->else_(query::val(else_value)); }

  // Build the final CaseExpr
  auto build() { return CaseExpr(std::move(when_thens_), std::move(else_expr_)); }

  // Friend access for template constructor
  template <typename>
  friend class TypedCaseBuilder;
};

// Keep the old CaseBuilder as an alias for backward compatibility
using CaseBuilder = TypedCaseBuilder<void>;

/// @brief Create a CASE expression with type checking
/// @return A TypedCaseBuilder
inline auto case_() {
  return TypedCaseBuilder<void>();
}

// Specialized as function for CaseExpr
inline auto as(CaseExpr&& expr, std::string alias) {
  // Create a shared_ptr directly with an in-place construction
  // This avoids the need to copy the CaseExpr
  auto expr_ptr = std::make_unique<CaseExpr>(std::move(expr));
  return AliasedColumn<CaseExpr>(std::shared_ptr<CaseExpr>(std::move(expr_ptr)), std::move(alias));
}

}  // namespace query
}  // namespace relx