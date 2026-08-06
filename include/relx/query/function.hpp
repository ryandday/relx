#pragma once

#include "column_expression.hpp"
#include "core.hpp"
#include "value.hpp"

#include <concepts>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace relx::query {

/// @brief Base class for SQL function expressions
/// @tparam Expr The expression type for the function argument
template <SqlExpr Expr>
class FunctionExpr : public ColumnExpression {
public:
  FunctionExpr(std::string name, Expr expr) : func_name_(std::move(name)), expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override { return func_name_ + "(" + expr_.to_sql() + ")"; }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

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

  constexpr std::string to_sql() const override { return func_name_ + "()"; }

  constexpr std::vector<bind_param> bind_params() const override { return {}; }

  std::string column_name() const override { return func_name_ + "()"; }

  std::string table_name() const override { return ""; }

private:
  std::string func_name_;
};

/// @brief Expression representing COUNT(*) in SQL
class CountAllExpr : public ColumnExpression {
public:
  constexpr std::string to_sql() const override { return "COUNT(*)"; }

  constexpr std::vector<bind_param> bind_params() const override { return {}; }

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

template <typename TableT, schema::fixed_string Name, typename ColumnT, typename... Modifiers>
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

  constexpr std::string to_sql() const override { return "DISTINCT " + expr_.to_sql(); }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

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

  constexpr std::string to_sql() const override {
    std::stringstream ss;
    ss << "COALESCE(" << first_.to_sql() << ", " << second_.to_sql();

    std::apply([&](const auto&... items) { ((ss << ", " << items.to_sql()), ...); }, rest_);

    ss << ")";
    return ss.str();
  }

  constexpr std::vector<bind_param> bind_params() const override {
    std::vector<bind_param> params;

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

/// @brief One WHEN ... THEN ... arm of a CASE expression, stored by value
template <ConditionExpr Cond, SqlExpr Then>
struct WhenThen {
  Cond cond;
  Then then;
};

/// @brief Tag type for a CASE expression without an ELSE branch
struct NoElse {};

namespace detail {

/// @brief Exposes value_type when the branch expression declares one, so an aliased
/// CASE can name a member of a synthesized row type (see row_type.hpp)
template <typename T>
struct case_result_type {};

template <typename T>
  requires requires { typename T::value_type; }
struct case_result_type<T> {
  using value_type = typename T::value_type;
};

template <typename... WhenThens>
struct first_then {
  using type = void;
};

template <ConditionExpr Cond, SqlExpr Then, typename... Rest>
struct first_then<WhenThen<Cond, Then>, Rest...> {
  using type = Then;
};

}  // namespace detail

/// @brief CASE expression with every arm stored by value in the type: copyable,
/// heap-free, and able to constant-evaluate in static_sql contexts
template <typename ElseT, typename... WhenThens>
class CaseExpr : public ColumnExpression,
                 public detail::case_result_type<typename detail::first_then<WhenThens...>::type> {
public:
  static_assert(sizeof...(WhenThens) > 0, "CASE requires at least one WHEN arm");

  constexpr CaseExpr(std::tuple<WhenThens...> when_thens, ElseT else_expr)
      : when_thens_(std::move(when_thens)), else_expr_(std::move(else_expr)) {}

  constexpr std::string to_sql() const override {
    std::string sql = "CASE";

    std::apply(
        [&](const auto&... arms) {
          ((sql += " WHEN (" + unwrapped(arms.cond.to_sql()) + ") THEN " + arms.then.to_sql()),
           ...);
        },
        when_thens_);

    if constexpr (!std::same_as<ElseT, NoElse>) {
      sql += " ELSE " + else_expr_.to_sql();
    }

    sql += " END";
    return sql;
  }

  constexpr std::vector<bind_param> bind_params() const override {
    std::vector<bind_param> params;
    const auto append = [&params](const auto& expr) {
      auto expr_params = expr.bind_params();
      params.insert(params.end(), expr_params.begin(), expr_params.end());
    };

    // Interleave condition and value parameters in the expected order
    std::apply([&](const auto&... arms) { ((append(arms.cond), append(arms.then)), ...); },
               when_thens_);

    if constexpr (!std::same_as<ElseT, NoElse>) {
      append(else_expr_);
    }

    return params;
  }

  constexpr std::string column_name() const override { return "CASE"; }

  constexpr std::string table_name() const override { return ""; }

private:
  // Conditions render with their own outer parentheses; CASE supplies the WHEN pair
  static constexpr std::string unwrapped(std::string cond) {
    if (cond.size() >= 2 && cond.front() == '(' && cond.back() == ')') {
      return cond.substr(1, cond.size() - 2);
    }
    return cond;
  }

  std::tuple<WhenThens...> when_thens_;
  [[no_unique_address]] ElseT else_expr_;
};

/// @brief Builder for CASE expressions. Each when()/else_() threads the arm into the
/// builder's type; ResultType pins the branch result type so mixed branches fail to
/// compile.
template <typename ResultType, typename ElseT, typename... WhenThens>
class TypedCaseBuilder {
public:
  constexpr TypedCaseBuilder()
    requires(sizeof...(WhenThens) == 0 && std::same_as<ElseT, NoElse>)
  = default;

  template <ConditionExpr Cond, SqlExpr Then>
  constexpr auto when(Cond when_cond, Then then) {
    if constexpr (!std::same_as<ResultType, void>) {
      static_assert(std::same_as<std::remove_cvref_t<Then>, std::remove_cvref_t<ResultType>>,
                    "All WHEN/THEN branches in CASE expressions must return the same type. "
                    "Mixed types in CASE branches can lead to runtime type errors.");
    }
    // The first WHEN establishes the result type for the checks above
    using Result = std::conditional_t<std::same_as<ResultType, void>, Then, ResultType>;
    return TypedCaseBuilder<Result, ElseT, WhenThens..., WhenThen<Cond, Then>>(
        std::tuple_cat(std::move(when_thens_), std::make_tuple(WhenThen<Cond, Then>{
                                                   std::move(when_cond), std::move(then)})),
        std::move(else_expr_));
  }

  // Specific overloads for common literal types to avoid template conflicts
  constexpr auto when(const ConditionExpr auto& when_cond, const char* then) {
    return this->when(when_cond, query::val(then));
  }

  constexpr auto when(const ConditionExpr auto& when_cond, const std::string& then) {
    return this->when(when_cond, query::val(then));
  }

  constexpr auto when(const ConditionExpr auto& when_cond, int then) {
    return this->when(when_cond, query::val(then));
  }

  constexpr auto when(const ConditionExpr auto& when_cond, long then) {
    return this->when(when_cond, query::val(then));
  }

  constexpr auto when(const ConditionExpr auto& when_cond, double then) {
    return this->when(when_cond, query::val(then));
  }

  constexpr auto when(const ConditionExpr auto& when_cond, float then) {
    return this->when(when_cond, query::val(then));
  }

  constexpr auto when(const ConditionExpr auto& when_cond, bool then) {
    return this->when(when_cond, query::val(then));
  }

  template <SqlExpr Else>
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(Else else_expr) {
    if constexpr (!std::same_as<ResultType, void>) {
      static_assert(std::same_as<std::remove_cvref_t<Else>, std::remove_cvref_t<ResultType>>,
                    "ELSE clause type must match the WHEN/THEN branch types in CASE expressions.");
    }

    return TypedCaseBuilder<ResultType, Else, WhenThens...>(std::move(when_thens_),
                                                            std::move(else_expr));
  }

  // Specific overloads for common literal types in else clause
  // Have to use underscore to avoid conflict with else keyword
  // but this is not allowed by clang-tidy.
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(const char* else_value) { return this->else_(query::val(else_value)); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(const std::string& else_value) {
    return this->else_(query::val(else_value));
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(int else_value) { return this->else_(query::val(else_value)); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(long else_value) { return this->else_(query::val(else_value)); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(double else_value) { return this->else_(query::val(else_value)); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(float else_value) { return this->else_(query::val(else_value)); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr auto else_(bool else_value) { return this->else_(query::val(else_value)); }

  // Build the final CaseExpr
  constexpr auto build() {
    static_assert(sizeof...(WhenThens) > 0, "CASE requires at least one WHEN arm");
    return CaseExpr<ElseT, WhenThens...>(std::move(when_thens_), std::move(else_expr_));
  }

private:
  constexpr TypedCaseBuilder(std::tuple<WhenThens...> when_thens, ElseT else_expr)
      : when_thens_(std::move(when_thens)), else_expr_(std::move(else_expr)) {}

  // Builders of every arm/result shape construct each other as arms accumulate
  template <typename, typename, typename...>
  friend class TypedCaseBuilder;

  std::tuple<WhenThens...> when_thens_;
  [[no_unique_address]] ElseT else_expr_;
};

/// @brief Create a CASE expression with type checking
/// @return A TypedCaseBuilder
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto case_() {
  return TypedCaseBuilder<void, NoElse>();
}

}  // namespace relx::query