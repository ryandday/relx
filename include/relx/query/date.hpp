#pragma once

#include "../schema/chrono_traits.hpp"
#include "../schema/column.hpp"
#include "arithmetic.hpp"
#include "column_expression.hpp"
#include "condition.hpp"
#include "core.hpp"
#include "date_concepts.hpp"
#include "function.hpp"
#include "value.hpp"

#include <chrono>
#include <concepts>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace relx::query {

/// @brief Binary date function expression (e.g., DATE_DIFF)
template <SqlExpr Left, SqlExpr Right>
class BinaryDateFunctionExpr : public ColumnExpression {
public:
  BinaryDateFunctionExpr(std::string func_name, std::string unit, Left left, Right right)
      : func_name_(std::move(func_name)), unit_(std::move(unit)), left_(std::move(left)),
        right_(std::move(right)) {}

  constexpr std::string to_sql() const override {
    // Generate PostgreSQL-compatible SQL for date differences. Units arrive
    // normalized to singular by date_diff(), which rejects unknown units.
    if (func_name_ == "DATE_DIFF") {
      if (unit_ == "year") {
        return "EXTRACT(YEAR FROM AGE(" + right_.to_sql() + ", " + left_.to_sql() + "))";
      } else if (unit_ == "day") {
        // For PostgreSQL, handle date vs timestamp arithmetic properly
        // If we're dealing with timestamps, convert to date difference in days
        // If we're dealing with dates, simple subtraction works
        return "(" + right_.to_sql() + "::date - " + left_.to_sql() + "::date)";
      } else if (unit_ == "second") {
        return "EXTRACT(EPOCH FROM (" + right_.to_sql() + " - " + left_.to_sql() + "))";
      } else if (unit_ == "month") {
        // TOTAL months, not the interval's month component (14 months apart must be
        // 14, not 2): years * 12 + months from AGE
        const std::string age = "AGE(" + right_.to_sql() + ", " + left_.to_sql() + ")";
        return "(EXTRACT(YEAR FROM " + age + ") * 12 + EXTRACT(MONTH FROM " + age + "))";
      } else if (unit_ == "hour") {
        return "EXTRACT(EPOCH FROM (" + right_.to_sql() + " - " + left_.to_sql() + "))/3600";
      } else if (unit_ == "minute") {
        return "EXTRACT(EPOCH FROM (" + right_.to_sql() + " - " + left_.to_sql() + "))/60";
      }
    }
    // Fallback to generic format for other functions
    return func_name_ + "('" + unit_ + "', " + left_.to_sql() + ", " + right_.to_sql() + ")";
  }

  constexpr std::vector<bind_param> bind_params() const override {
    auto left_params = left_.bind_params();
    auto right_params = right_.bind_params();
    left_params.insert(left_params.end(), right_params.begin(), right_params.end());
    return left_params;
  }

  std::string column_name() const override { return func_name_ + "_" + unit_; }

  std::string table_name() const override {
    if constexpr (std::is_base_of_v<ColumnExpression, Left>) {
      auto table = left_.table_name();
      if (!table.empty()) {
        return table;
      }
    }
    if constexpr (std::is_base_of_v<ColumnExpression, Right>) {
      return right_.table_name();
    }
    return "";
  }

  // Operator overloads for comparisons with literals
  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator==(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<BinaryDateFunctionExpr, decltype(val_expr), "=">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator!=(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<BinaryDateFunctionExpr, decltype(val_expr), "!=">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator>(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<BinaryDateFunctionExpr, decltype(val_expr), ">">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator<(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<BinaryDateFunctionExpr, decltype(val_expr), "<">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator>=(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<BinaryDateFunctionExpr, decltype(val_expr), ">=">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator<=(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<BinaryDateFunctionExpr, decltype(val_expr), "<=">(*this, val_expr);
  }

  // Arithmetic operator overloads
  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator*(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<BinaryDateFunctionExpr, decltype(val_expr)>(*this, "*", val_expr);
  }

  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator+(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<BinaryDateFunctionExpr, decltype(val_expr)>(*this, "+", val_expr);
  }

  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator-(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<BinaryDateFunctionExpr, decltype(val_expr)>(*this, "-", val_expr);
  }

  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator/(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<BinaryDateFunctionExpr, decltype(val_expr)>(*this, "/", val_expr);
  }

private:
  std::string func_name_;
  std::string unit_;
  Left left_;
  Right right_;
};

/// @brief Unary date function expression with unit (e.g., EXTRACT)
template <SqlExpr Expr>
class UnaryDateFunctionExpr : public ColumnExpression {
public:
  UnaryDateFunctionExpr(std::string func_name, std::string unit, Expr expr)
      : func_name_(std::move(func_name)), unit_(std::move(unit)), expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override {
    if (func_name_ == "EXTRACT") {
      return "EXTRACT(" + unit_ + " FROM " + expr_.to_sql() + ")";
    }
    if (func_name_ == "ABS") {
      return "ABS(" + expr_.to_sql() + ")";
    }
    return func_name_ + "('" + unit_ + "', " + expr_.to_sql() + ")";
  }

  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }

  std::string column_name() const override { return func_name_ + "_" + unit_; }

  std::string table_name() const override {
    if constexpr (std::is_base_of_v<ColumnExpression, Expr>) {
      return expr_.table_name();
    }
    return "";
  }

  // Operator overloads for comparisons with literals
  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator==(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<UnaryDateFunctionExpr, decltype(val_expr), "=">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator!=(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<UnaryDateFunctionExpr, decltype(val_expr), "!=">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator>(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<UnaryDateFunctionExpr, decltype(val_expr), ">">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator<(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<UnaryDateFunctionExpr, decltype(val_expr), "<">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator>=(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<UnaryDateFunctionExpr, decltype(val_expr), ">=">(*this, val_expr);
  }

  template <typename LiteralT>
    requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
             std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
  auto operator<=(LiteralT&& literal) const {
    auto val_expr = val(std::forward<LiteralT>(literal));
    return BinaryCondition<UnaryDateFunctionExpr, decltype(val_expr), "<=">(*this, val_expr);
  }

  // Arithmetic operator overloads
  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator*(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<UnaryDateFunctionExpr, decltype(val_expr)>(*this, "*", val_expr);
  }

  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator+(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<UnaryDateFunctionExpr, decltype(val_expr)>(*this, "+", val_expr);
  }

  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator-(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<UnaryDateFunctionExpr, decltype(val_expr)>(*this, "-", val_expr);
  }

  template <typename NumericT>
    requires std::is_arithmetic_v<std::remove_cvref_t<NumericT>>
  auto operator/(NumericT&& literal) const {
    auto val_expr = val(std::forward<NumericT>(literal));
    return ArithmeticExpr<UnaryDateFunctionExpr, decltype(val_expr)>(*this, "/", val_expr);
  }

private:
  std::string func_name_;
  std::string unit_;
  Expr expr_;
};

/// @brief Date interval expression for date arithmetic
class IntervalExpr : public ColumnExpression {
public:
  explicit IntervalExpr(std::string interval) : interval_(std::move(interval)) {}

  constexpr std::string to_sql() const override { return "INTERVAL '" + interval_ + "'"; }

  constexpr std::vector<bind_param> bind_params() const override { return {}; }

  std::string column_name() const override { return "INTERVAL"; }

  std::string table_name() const override { return ""; }

private:
  std::string interval_;
};

/// @brief Date addition/subtraction expression
template <SqlExpr DateExpr, SqlExpr IntervalExpr>
class DateArithmeticExpr : public ColumnExpression {
public:
  DateArithmeticExpr(DateExpr date_expr, std::string op, IntervalExpr interval_expr)
      : date_expr_(std::move(date_expr)), op_(std::move(op)),
        interval_expr_(std::move(interval_expr)) {}

  constexpr std::string to_sql() const override {
    return "(" + date_expr_.to_sql() + " " + op_ + " " + interval_expr_.to_sql() + ")";
  }

  constexpr std::vector<bind_param> bind_params() const override {
    auto date_params = date_expr_.bind_params();
    auto interval_params = interval_expr_.bind_params();
    date_params.insert(date_params.end(), interval_params.begin(), interval_params.end());
    return date_params;
  }

  std::string column_name() const override {
    std::string date_name = "expr";
    if constexpr (std::is_base_of_v<ColumnExpression, DateExpr>) {
      date_name = date_expr_.column_name();
    }
    return "(" + date_name + "_" + op_ + "_INTERVAL)";
  }

  std::string table_name() const override {
    if constexpr (std::is_base_of_v<ColumnExpression, DateExpr>) {
      return date_expr_.table_name();
    }
    return "";
  }

private:
  DateExpr date_expr_;
  std::string op_;
  IntervalExpr interval_expr_;
};

/// @brief Current date/time functions (no arguments)
class CurrentDateTimeExpr : public ColumnExpression {
public:
  explicit CurrentDateTimeExpr(std::string func_name) : func_name_(std::move(func_name)) {}

  constexpr std::string to_sql() const override { return func_name_; }

  constexpr std::vector<bind_param> bind_params() const override { return {}; }

  std::string column_name() const override { return func_name_; }

  std::string table_name() const override { return ""; }

  // Comparison operators with date columns
  template <typename T>
    requires date_checking::DateTimeColumn<T>
  auto operator>(const T& column) const {
    return BinaryCondition<CurrentDateTimeExpr, decltype(to_expr(column)), ">">(*this,
                                                                                to_expr(column));
  }

  template <typename T>
    requires date_checking::DateTimeColumn<T>
  auto operator<(const T& column) const {
    return BinaryCondition<CurrentDateTimeExpr, decltype(to_expr(column)), "<">(*this,
                                                                                to_expr(column));
  }

  template <typename T>
    requires date_checking::DateTimeColumn<T>
  auto operator>=(const T& column) const {
    return BinaryCondition<CurrentDateTimeExpr, decltype(to_expr(column)), ">=">(*this,
                                                                                 to_expr(column));
  }

  template <typename T>
    requires date_checking::DateTimeColumn<T>
  auto operator<=(const T& column) const {
    return BinaryCondition<CurrentDateTimeExpr, decltype(to_expr(column)), "<=">(*this,
                                                                                 to_expr(column));
  }

  template <typename T>
    requires date_checking::DateTimeColumn<T>
  auto operator==(const T& column) const {
    return BinaryCondition<CurrentDateTimeExpr, decltype(to_expr(column)), "=">(*this,
                                                                                to_expr(column));
  }

  template <typename T>
    requires date_checking::DateTimeColumn<T>
  auto operator!=(const T& column) const {
    return BinaryCondition<CurrentDateTimeExpr, decltype(to_expr(column)), "!=">(*this,
                                                                                 to_expr(column));
  }

  // Comparison operators with other date expressions
  template <SqlExpr Expr>
  auto operator>(Expr expr) const {
    return BinaryCondition<CurrentDateTimeExpr, Expr, ">">(*this, std::move(expr));
  }

  template <SqlExpr Expr>
  auto operator<(Expr expr) const {
    return BinaryCondition<CurrentDateTimeExpr, Expr, "<">(*this, std::move(expr));
  }

  template <SqlExpr Expr>
  auto operator>=(Expr expr) const {
    return BinaryCondition<CurrentDateTimeExpr, Expr, ">=">(*this, std::move(expr));
  }

  template <SqlExpr Expr>
  auto operator<=(Expr expr) const {
    return BinaryCondition<CurrentDateTimeExpr, Expr, "<=">(*this, std::move(expr));
  }

  template <SqlExpr Expr>
  auto operator==(Expr expr) const {
    return BinaryCondition<CurrentDateTimeExpr, Expr, "=">(*this, std::move(expr));
  }

  template <SqlExpr Expr>
  auto operator!=(Expr expr) const {
    return BinaryCondition<CurrentDateTimeExpr, Expr, "!=">(*this, std::move(expr));
  }

private:
  std::string func_name_;
};

namespace detail {

/// @brief Normalize a date_diff unit to its singular form and validate it. Unknown
/// units once fell through to a nonexistent DATE_DIFF(...) SQL function that only
/// failed at execution time.
inline std::string normalize_date_diff_unit(std::string_view unit) {
  std::string normalized(unit);
  if (!normalized.empty() && normalized.back() == 's') {
    normalized.pop_back();
  }
  for (const std::string_view known : {"year", "month", "day", "hour", "minute", "second"}) {
    if (normalized == known) {
      return normalized;
    }
  }
  throw std::invalid_argument("date_diff: unknown unit '" + std::string(unit) +
                              "' (expected years, months, days, hours, minutes, or seconds)");
}

}  // namespace detail

/// @brief DATE_DIFF function - calculates difference between two dates
/// @tparam Expr1 First date expression type
/// @tparam Expr2 Second date expression type
/// @param unit Time unit for the difference (e.g., "days", "months", "years";
/// singular forms also accepted). Unknown units throw std::invalid_argument.
/// @param date1 First date expression
/// @param date2 Second date expression
/// @return A BinaryDateFunctionExpr representing DATE_DIFF(unit, date1, date2)
template <SqlExpr Expr1, SqlExpr Expr2>
auto date_diff(std::string_view unit, Expr1 date1, Expr2 date2) {
  return BinaryDateFunctionExpr<Expr1, Expr2>("DATE_DIFF", detail::normalize_date_diff_unit(unit),
                                              std::move(date1), std::move(date2));
}

// Overload for column types with type checking
template <typename T1, typename T2>
  requires date_checking::DateTimeColumn<T1> && date_checking::DateTimeColumn<T2>
auto date_diff(std::string_view unit, const T1& col1, const T2& col2) {
  return date_diff(unit, to_expr(col1), to_expr(col2));
}

// Overload for column and expression
template <typename T, SqlExpr Expr>
  requires date_checking::DateTimeColumn<T>
auto date_diff(std::string_view unit, const T& column, Expr expr) {
  return date_diff(unit, to_expr(column), std::move(expr));
}

// Overload for expression and column
template <SqlExpr Expr, typename T>
  requires date_checking::DateTimeColumn<T>
auto date_diff(std::string_view unit, Expr expr, const T& column) {
  return date_diff(unit, std::move(expr), to_expr(column));
}

/// @brief DATE_ADD function - adds an interval to a date
/// @tparam DateExpr Date expression type
/// @param date_expr Date expression
/// @param interval_expr Interval expression (created with interval())
/// @return A DateArithmeticExpr representing date + interval
template <SqlExpr DateExpr>
auto date_add(DateExpr date_expr, IntervalExpr interval_expr) {
  return DateArithmeticExpr<DateExpr, IntervalExpr>(std::move(date_expr), "+",
                                                    std::move(interval_expr));
}

// Overload for column types with type checking
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto date_add(const T& column, IntervalExpr interval_expr) {
  return date_add(to_expr(column), std::move(interval_expr));
}

/// @brief DATE_SUB function - subtracts an interval from a date
/// @tparam DateExpr Date expression type
/// @param date_expr Date expression
/// @param interval_expr Interval expression (created with interval())
/// @return A DateArithmeticExpr representing date - interval
template <SqlExpr DateExpr>
auto date_sub(DateExpr date_expr, IntervalExpr interval_expr) {
  return DateArithmeticExpr<DateExpr, IntervalExpr>(std::move(date_expr), "-",
                                                    std::move(interval_expr));
}

// Overload for column types with type checking
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto date_sub(const T& column, IntervalExpr interval_expr) {
  return date_sub(to_expr(column), std::move(interval_expr));
}

namespace detail {

/// @brief Validate a date-part unit name (extract/date_trunc): lowercase field
/// identifiers only. These strings are spliced into SQL text, so anything else is
/// rejected up front instead of reaching the server.
inline std::string validated_date_unit(std::string_view unit) {
  if (unit.empty()) {
    throw std::invalid_argument("date unit cannot be empty");
  }
  for (const char c : unit) {
    if (!((c >= 'a' && c <= 'z') || c == '_')) {
      throw std::invalid_argument("invalid date unit '" + std::string(unit) +
                                  "': units are lowercase field names like year, month, dow");
    }
  }
  return std::string(unit);
}

/// @brief Validate an interval literal body ("1 day", "3 months"). Spliced into
/// INTERVAL '...' - quotes and backslashes are rejected, not escaped.
inline std::string validated_interval(std::string_view interval_str) {
  if (interval_str.empty()) {
    throw std::invalid_argument("interval cannot be empty");
  }
  for (const char c : interval_str) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == ' ' || c == '.' || c == '+' || c == '-' || c == ':';
    if (!ok) {
      throw std::invalid_argument("invalid interval '" + std::string(interval_str) +
                                  "': expected forms like '1 day' or '01:30:00'");
    }
  }
  return std::string(interval_str);
}

}  // namespace detail

/// @brief EXTRACT function - extracts a date part from a date
/// @tparam Expr Date expression type
/// @param unit Date part to extract (e.g., "year", "month", "day", "hour", "minute", "second",
/// "dow"). Invalid unit text throws std::invalid_argument.
/// @param expr Date expression
/// @return A UnaryDateFunctionExpr representing EXTRACT(unit FROM expr)
template <SqlExpr Expr>
auto extract(std::string_view unit, Expr expr) {
  return UnaryDateFunctionExpr<Expr>("EXTRACT", detail::validated_date_unit(unit), std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto extract(std::string_view unit, const T& column) {
  return extract(unit, to_expr(column));
}

/// @brief DATE_TRUNC function - truncates a date to specified precision
/// @tparam Expr Date expression type
/// @param unit Precision to truncate to (e.g., "year", "month", "day", "hour", "minute")
/// @param expr Date expression
/// @return A UnaryDateFunctionExpr representing DATE_TRUNC(unit, expr)
template <SqlExpr Expr>
auto date_trunc(std::string_view unit, Expr expr) {
  return UnaryDateFunctionExpr<Expr>("DATE_TRUNC", detail::validated_date_unit(unit),
                                     std::move(expr));
}

// Overload for column types with type checking
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto date_trunc(std::string_view unit, const T& column) {
  return date_trunc(unit, to_expr(column));
}

/// @brief Create an interval expression
/// @param interval_str Interval string (e.g., "1 day", "3 months", "2 years").
/// Quotes and other splice-capable characters throw std::invalid_argument.
/// @return An IntervalExpr representing the interval
inline auto interval(std::string_view interval_str) {
  return IntervalExpr(detail::validated_interval(interval_str));
}

/// @brief CURRENT_DATE function - returns the current date
/// @return A CurrentDateTimeExpr representing CURRENT_DATE
inline auto current_date() {
  return CurrentDateTimeExpr("CURRENT_DATE");
}

/// @brief CURRENT_TIME function - returns the current time
/// @return A CurrentDateTimeExpr representing CURRENT_TIME
inline auto current_time() {
  return CurrentDateTimeExpr("CURRENT_TIME");
}

/// @brief CURRENT_TIMESTAMP function - returns the current timestamp
/// @return A CurrentDateTimeExpr representing CURRENT_TIMESTAMP
inline auto current_timestamp() {
  return CurrentDateTimeExpr("CURRENT_TIMESTAMP");
}

/// @brief NOW function - alias for CURRENT_TIMESTAMP
/// @return A CurrentDateTimeExpr representing NOW()
inline auto now() {
  return CurrentDateTimeExpr("NOW()");
}

// Helper functions for common date operations

/// @brief Calculate age in years between birth date and current date
/// @tparam T Date column type
/// @param birth_date_column Birth date column
/// @return Date difference expression in years
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto age_in_years(const T& birth_date_column) {
  return date_diff("year", birth_date_column, current_date());
}

// Overload for any date expression
template <SqlExpr Expr>
auto age_in_years(Expr expr) {
  return date_diff("year", std::move(expr), current_date());
}

/// @brief Calculate days since a date
/// @tparam T Date column type
/// @param date_column Date column
/// @return Date difference expression in days
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto days_since(const T& date_column) {
  return date_diff("day", date_column, current_date());
}

// Overload for any date expression
template <SqlExpr Expr>
auto days_since(Expr expr) {
  return date_diff("day", std::move(expr), current_date());
}

/// @brief Calculate days until a date
/// @tparam T Date column type
/// @param date_column Date column
/// @return Date difference expression in days
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto days_until(const T& date_column) {
  return date_diff("day", current_date(), date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto days_until(Expr expr) {
  return date_diff("day", current_date(), std::move(expr));
}

/// @brief Get the start of the year for a date
/// @tparam T Date column type
/// @param date_column Date column
/// @return Date truncation expression to year
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto start_of_year(const T& date_column) {
  return date_trunc("year", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto start_of_year(Expr expr) {
  return date_trunc("year", std::move(expr));
}

/// @brief Get the start of the month for a date
/// @tparam T Date column type
/// @param date_column Date column
/// @return Date truncation expression to month
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto start_of_month(const T& date_column) {
  return date_trunc("month", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto start_of_month(Expr expr) {
  return date_trunc("month", std::move(expr));
}

/// @brief Get the start of the day for a date
/// @tparam T Date column type
/// @param date_column Date column
/// @return Date truncation expression to day
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto start_of_day(const T& date_column) {
  return date_trunc("day", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto start_of_day(Expr expr) {
  return date_trunc("day", std::move(expr));
}

/// @brief Get the year from a date
/// @tparam T Date column or expression type
/// @param date_column Date column
/// @return Extract expression for year
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto year(const T& date_column) {
  return extract("year", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto year(Expr expr) {
  return extract("year", std::move(expr));
}

/// @brief Get the month from a date
/// @tparam T Date column or expression type
/// @param date_column Date column
/// @return Extract expression for month
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto month(const T& date_column) {
  return extract("month", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto month(Expr expr) {
  return extract("month", std::move(expr));
}

/// @brief Get the day from a date
/// @tparam T Date column or expression type
/// @param date_column Date column
/// @return Extract expression for day
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto day(const T& date_column) {
  return extract("day", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto day(Expr expr) {
  return extract("day", std::move(expr));
}

/// @brief Get the day of week from a date (0=Sunday, 1=Monday, etc.)
/// @tparam T Date column type
/// @param date_column Date column
/// @return Extract expression for day of week
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto day_of_week(const T& date_column) {
  return extract("dow", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto day_of_week(Expr expr) {
  return extract("dow", std::move(expr));
}

/// @brief Get the day of year from a date (1-366)
/// @tparam T Date column type
/// @param date_column Date column
/// @return Extract expression for day of year
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto day_of_year(const T& date_column) {
  return extract("doy", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto day_of_year(Expr expr) {
  return extract("doy", std::move(expr));
}

/// @brief Get the hour from a timestamp
/// @tparam T Date column type
/// @param date_column Date column
/// @return Extract expression for hour
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto hour(const T& date_column) {
  return extract("hour", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto hour(Expr expr) {
  return extract("hour", std::move(expr));
}

/// @brief Get the minute from a timestamp
/// @tparam T Date column type
/// @param date_column Date column
/// @return Extract expression for minute
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto minute(const T& date_column) {
  return extract("minute", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto minute(Expr expr) {
  return extract("minute", std::move(expr));
}

/// @brief Get the second from a timestamp
/// @tparam T Date column type
/// @param date_column Date column
/// @return Extract expression for second
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto second(const T& date_column) {
  return extract("second", date_column);
}

// Overload for any date expression
template <SqlExpr Expr>
auto second(Expr expr) {
  return extract("second", std::move(expr));
}

// Global operator overloads for date arithmetic with intervals

/// @brief Addition operator for date column + interval
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto operator+(const T& date_column, const IntervalExpr& interval_expr) {
  return DateArithmeticExpr<decltype(to_expr(date_column)), IntervalExpr>(to_expr(date_column), "+",
                                                                          interval_expr);
}

/// @brief Addition operator for date expression + interval
template <SqlExpr DateExpr>
auto operator+(DateExpr date_expr, const IntervalExpr& interval_expr) {
  return DateArithmeticExpr<DateExpr, IntervalExpr>(std::move(date_expr), "+", interval_expr);
}

/// @brief Subtraction operator for date column - interval
template <typename T>
  requires date_checking::DateTimeColumn<T>
auto operator-(const T& date_column, const IntervalExpr& interval_expr) {
  return DateArithmeticExpr<decltype(to_expr(date_column)), IntervalExpr>(to_expr(date_column), "-",
                                                                          interval_expr);
}

/// @brief Subtraction operator for date expression - interval
template <SqlExpr DateExpr>
auto operator-(DateExpr date_expr, const IntervalExpr& interval_expr) {
  return DateArithmeticExpr<DateExpr, IntervalExpr>(std::move(date_expr), "-", interval_expr);
}

/// @brief Addition operator for DateArithmeticExpr + interval (chaining)
template <SqlExpr DateExpr, SqlExpr IntervalExpr1>
auto operator+(const DateArithmeticExpr<DateExpr, IntervalExpr1>& date_expr,
               const IntervalExpr& interval_expr) {
  return DateArithmeticExpr<DateArithmeticExpr<DateExpr, IntervalExpr1>, IntervalExpr>(
      date_expr, "+", interval_expr);
}

/// @brief Subtraction operator for DateArithmeticExpr - interval (chaining)
template <SqlExpr DateExpr, SqlExpr IntervalExpr1>
auto operator-(const DateArithmeticExpr<DateExpr, IntervalExpr1>& date_expr,
               const IntervalExpr& interval_expr) {
  return DateArithmeticExpr<DateArithmeticExpr<DateExpr, IntervalExpr1>, IntervalExpr>(
      date_expr, "-", interval_expr);
}

/// @brief ABS function for SQL expressions
/// @tparam Expr SQL expression type
/// @param expr SQL expression
/// @return A UnaryDateFunctionExpr representing ABS(expr)
template <SqlExpr Expr>
auto abs(Expr expr) {
  return UnaryDateFunctionExpr<Expr>("ABS", "", std::move(expr));
}

// Arithmetic operators for UnaryDateFunctionExpr types

/// @brief Subtraction operator for two UnaryDateFunctionExpr
template <SqlExpr Expr1, SqlExpr Expr2>
auto operator-(const UnaryDateFunctionExpr<Expr1>& left,
               const UnaryDateFunctionExpr<Expr2>& right) {
  return ArithmeticExpr<UnaryDateFunctionExpr<Expr1>, UnaryDateFunctionExpr<Expr2>>(left, "-",
                                                                                    right);
}

/// @brief Addition operator for two UnaryDateFunctionExpr
template <SqlExpr Expr1, SqlExpr Expr2>
auto operator+(const UnaryDateFunctionExpr<Expr1>& left,
               const UnaryDateFunctionExpr<Expr2>& right) {
  return ArithmeticExpr<UnaryDateFunctionExpr<Expr1>, UnaryDateFunctionExpr<Expr2>>(left, "+",
                                                                                    right);
}

/// @brief Specialization for time_point values: bind the raw ISO 8601 text
/// (the traits' to_sql_string is SQL-literal syntax and must not leak into binds)
template <>
class Value<std::chrono::system_clock::time_point> : public SqlExpression {
public:
  using value_type = std::chrono::system_clock::time_point;

  explicit Value(std::chrono::system_clock::time_point value) : value_(value) {}

  constexpr std::string to_sql() const override { return "?"; }

  constexpr std::vector<bind_param> bind_params() const override {
    // Typed parameter: the text form reuses the traits' formatting (stripping the
    // SQL-literal quotes), and make_bind_param adds the timestamptz kind + binary
    // wire bytes so binary-protocol paths skip server-side text parsing
    std::string quoted = schema::column_traits<value_type>::to_sql_string(value_);
    return {relx::make_bind_param(value_, quoted.substr(1, quoted.size() - 2))};
  }

  const value_type& value() const { return value_; }

private:
  value_type value_;
};

/// @brief Specialization for year_month_day values: bind the raw ISO 8601 date text
/// (the traits' to_sql_string is SQL-literal syntax and must not leak into binds)
template <>
class Value<std::chrono::year_month_day> : public SqlExpression {
public:
  using value_type = std::chrono::year_month_day;

  explicit Value(std::chrono::year_month_day value) : value_(value) {}

  constexpr std::string to_sql() const override { return "?"; }

  constexpr std::vector<bind_param> bind_params() const override {
    // Typed parameter: text form from the traits (sans SQL-literal quotes), plus the
    // date kind + binary wire bytes via make_bind_param
    std::string quoted = schema::column_traits<value_type>::to_sql_string(value_);
    return {relx::make_bind_param(value_, quoted.substr(1, quoted.size() - 2))};
  }

  const value_type& value() const { return value_; }

private:
  value_type value_;
};

/// @brief Helper to create a value expression from a year_month_day
inline auto val(std::chrono::year_month_day ymd) {
  return Value<std::chrono::year_month_day>(ymd);
}

/// @brief Helper to create a value expression from a time_point
inline auto val(std::chrono::system_clock::time_point tp) {
  return Value<std::chrono::system_clock::time_point>(tp);
}

}  // namespace relx::query
