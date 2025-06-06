#pragma once

#include "column_expression.hpp"
#include "core.hpp"
#include "operators.hpp"
#include "schema_adapter.hpp"
#include "value.hpp"

#include <type_traits>

namespace relx::query {

/// @brief Type checking for arithmetic operations
namespace arithmetic_checking {

/// @brief Concept for numeric types suitable for arithmetic operations
template <typename T>
concept Arithmetic = std::is_arithmetic_v<std::remove_cvref_t<T>> &&
                     !std::same_as<std::remove_cvref_t<T>, bool>;

/// @brief Helper to extract column type for arithmetic checking
template <typename T>
struct extract_arithmetic_type {
  using type = T;
};

template <typename TableT, schema::FixedString Name, typename ColumnT, typename... Modifiers>
struct extract_arithmetic_type<schema::column<TableT, Name, ColumnT, Modifiers...>> {
  using type = ColumnT;
};

template <typename T>
using extract_arithmetic_type_t = typename extract_arithmetic_type<T>::type;

/// @brief Helper to extract base type from optional
template <typename T>
struct remove_optional {
  using type = T;
};

template <typename T>
struct remove_optional<std::optional<T>> {
  using type = T;
};

template <typename T>
using remove_optional_t = typename remove_optional<T>::type;

/// @brief Error message for invalid arithmetic operations
inline constexpr std::string_view arithmetic_error_message =
    "Arithmetic operations (+, -, *, /) can only be performed on numeric columns (int, long, "
    "float, double, etc.). "
    "String, boolean, and other non-numeric types cannot be used in arithmetic expressions.";
}  // namespace arithmetic_checking

/// @brief Binary arithmetic expression
template <SqlExpr Left, SqlExpr Right>
class ArithmeticExpr : public ColumnExpression {
public:
  ArithmeticExpr(Left left, std::string op, Right right)
      : left_(std::move(left)), op_(std::move(op)), right_(std::move(right)) {}

  std::string to_sql() const override {
    return "(" + left_.to_sql() + " " + op_ + " " + right_.to_sql() + ")";
  }

  std::vector<std::string> bind_params() const override {
    auto left_params = left_.bind_params();
    auto right_params = right_.bind_params();
    left_params.insert(left_params.end(), right_params.begin(), right_params.end());
    return left_params;
  }

  std::string column_name() const override {
    std::string left_name;
    std::string right_name;

    // Get column names, handling both ColumnExpression and other SqlExpr types
    if constexpr (std::is_base_of_v<ColumnExpression, Left>) {
      left_name = left_.column_name();
    } else {
      left_name = "expr";
    }

    if constexpr (std::is_base_of_v<ColumnExpression, Right>) {
      right_name = right_.column_name();
    } else {
      right_name = "expr";
    }

    return "(" + left_name + "_" + op_ + "_" + right_name + ")";
  }

  std::string table_name() const override {
    // For arithmetic expressions, return the table name of the first operand that has one
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

private:
  Left left_;
  std::string op_;
  Right right_;
};

/// @brief Addition operator for columns with type checking
template <typename TableT1, schema::FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, schema::FixedString Name2, typename T2, typename... Modifiers2>
auto operator+(const schema::column<TableT1, Name1, T1, Modifiers1...>& left,
               const schema::column<TableT2, Name2, T2, Modifiers2...>& right) {
  // Extract base types, removing optional wrapper if present
  using LeftBaseType = arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = arithmetic_checking::remove_optional_t<T2>;

  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  auto right_expr = to_expr(right);
  return ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "+",
                                                                   std::move(right_expr));
}

/// @brief Addition operator for column with value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(const schema::column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(
      arithmetic_checking::Arithmetic<ValueT>,
      "Arithmetic operations require numeric values. Cannot add non-numeric values to columns.");

  auto left_expr = to_expr(left);
  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "+",
                                                                   std::move(value_expr));
}

/// @brief Addition operator for value with column (reversed)
template <typename ValueT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(ValueT&& value, const schema::column<TableT, Name, T, Modifiers...>& right) {
  return right + std::forward<ValueT>(value);
}

/// @brief Subtraction operator for columns
template <typename TableT1, schema::FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, schema::FixedString Name2, typename T2, typename... Modifiers2>
auto operator-(const schema::column<TableT1, Name1, T1, Modifiers1...>& left,
               const schema::column<TableT2, Name2, T2, Modifiers2...>& right) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = arithmetic_checking::remove_optional_t<T2>;

  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  auto right_expr = to_expr(right);
  return ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "-",
                                                                   std::move(right_expr));
}

/// @brief Subtraction operator for column with value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(const schema::column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values. Cannot subtract non-numeric values "
                "from columns.");

  auto left_expr = to_expr(left);
  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "-",
                                                                   std::move(value_expr));
}

/// @brief Subtraction operator for value with column (reversed)
template <typename ValueT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(ValueT&& value, const schema::column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  auto right_expr = to_expr(right);
  return ArithmeticExpr<decltype(value_expr), decltype(right_expr)>(std::move(value_expr), "-",
                                                                    std::move(right_expr));
}

/// @brief Multiplication operator for columns
template <typename TableT1, schema::FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, schema::FixedString Name2, typename T2, typename... Modifiers2>
auto operator*(const schema::column<TableT1, Name1, T1, Modifiers1...>& left,
               const schema::column<TableT2, Name2, T2, Modifiers2...>& right) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = arithmetic_checking::remove_optional_t<T2>;

  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  auto right_expr = to_expr(right);
  return ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "*",
                                                                   std::move(right_expr));
}

/// @brief Multiplication operator for column with value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(const schema::column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values. Cannot multiply columns by "
                "non-numeric values.");

  auto left_expr = to_expr(left);
  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "*",
                                                                   std::move(value_expr));
}

/// @brief Multiplication operator for value with column (reversed)
template <typename ValueT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(ValueT&& value, const schema::column<TableT, Name, T, Modifiers...>& right) {
  return right * std::forward<ValueT>(value);
}

/// @brief Division operator for columns
template <typename TableT1, schema::FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, schema::FixedString Name2, typename T2, typename... Modifiers2>
auto operator/(const schema::column<TableT1, Name1, T1, Modifiers1...>& left,
               const schema::column<TableT2, Name2, T2, Modifiers2...>& right) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = arithmetic_checking::remove_optional_t<T2>;

  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  auto right_expr = to_expr(right);
  return ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "/",
                                                                   std::move(right_expr));
}

/// @brief Division operator for column with value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(const schema::column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(
      arithmetic_checking::Arithmetic<ValueT>,
      "Arithmetic operations require numeric values. Cannot divide columns by non-numeric values.");

  auto left_expr = to_expr(left);
  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "/",
                                                                   std::move(value_expr));
}

/// @brief Division operator for value with column (reversed)
template <typename ValueT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(ValueT&& value, const schema::column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  auto right_expr = to_expr(right);
  return ArithmeticExpr<decltype(value_expr), decltype(right_expr)>(std::move(value_expr), "/",
                                                                    std::move(right_expr));
}

// Arithmetic operators for ArithmeticExpr to support chaining

/// @brief Addition operator for ArithmeticExpr with column
template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
auto operator+(const ArithmeticExpr<Left, Right>& left_expr,
               const schema::column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto right_expr = to_expr(right);
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(right_expr)>(left_expr, "+",
                                                                           std::move(right_expr));
}

/// @brief Addition operator for column with ArithmeticExpr
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          SqlExpr Left, SqlExpr Right>
auto operator+(const schema::column<TableT, Name, T, Modifiers...>& left,
               const ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  return ArithmeticExpr<decltype(left_expr), ArithmeticExpr<Left, Right>>(std::move(left_expr), "+",
                                                                          right_expr);
}

/// @brief Addition operator for ArithmeticExpr with value
template <SqlExpr Left, SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(const ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(value_expr)>(left_expr, "+",
                                                                           std::move(value_expr));
}

/// @brief Addition operator for value with ArithmeticExpr
template <typename ValueT, SqlExpr Left, SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(ValueT&& value, const ArithmeticExpr<Left, Right>& right_expr) {
  return right_expr + std::forward<ValueT>(value);
}

/// @brief Subtraction operator for ArithmeticExpr with column
template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
auto operator-(const ArithmeticExpr<Left, Right>& left_expr,
               const schema::column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto right_expr = to_expr(right);
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(right_expr)>(left_expr, "-",
                                                                           std::move(right_expr));
}

/// @brief Subtraction operator for column with ArithmeticExpr
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          SqlExpr Left, SqlExpr Right>
auto operator-(const schema::column<TableT, Name, T, Modifiers...>& left,
               const ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  return ArithmeticExpr<decltype(left_expr), ArithmeticExpr<Left, Right>>(std::move(left_expr), "-",
                                                                          right_expr);
}

/// @brief Subtraction operator for ArithmeticExpr with value
template <SqlExpr Left, SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(const ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(value_expr)>(left_expr, "-",
                                                                           std::move(value_expr));
}

/// @brief Subtraction operator for value with ArithmeticExpr
template <typename ValueT, SqlExpr Left, SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(ValueT&& value, const ArithmeticExpr<Left, Right>& right_expr) {
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<decltype(value_expr), ArithmeticExpr<Left, Right>>(std::move(value_expr),
                                                                           "-", right_expr);
}

/// @brief Multiplication operator for ArithmeticExpr with column
template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
auto operator*(const ArithmeticExpr<Left, Right>& left_expr,
               const schema::column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto right_expr = to_expr(right);
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(right_expr)>(left_expr, "*",
                                                                           std::move(right_expr));
}

/// @brief Multiplication operator for column with ArithmeticExpr
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          SqlExpr Left, SqlExpr Right>
auto operator*(const schema::column<TableT, Name, T, Modifiers...>& left,
               const ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  return ArithmeticExpr<decltype(left_expr), ArithmeticExpr<Left, Right>>(std::move(left_expr), "*",
                                                                          right_expr);
}

/// @brief Multiplication operator for ArithmeticExpr with value
template <SqlExpr Left, SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(const ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(value_expr)>(left_expr, "*",
                                                                           std::move(value_expr));
}

/// @brief Multiplication operator for value with ArithmeticExpr
template <typename ValueT, SqlExpr Left, SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(ValueT&& value, const ArithmeticExpr<Left, Right>& right_expr) {
  return right_expr * std::forward<ValueT>(value);
}

/// @brief Division operator for ArithmeticExpr with column
template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
auto operator/(const ArithmeticExpr<Left, Right>& left_expr,
               const schema::column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<RightBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto right_expr = to_expr(right);
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(right_expr)>(left_expr, "/",
                                                                           std::move(right_expr));
}

/// @brief Division operator for column with ArithmeticExpr
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          SqlExpr Left, SqlExpr Right>
auto operator/(const schema::column<TableT, Name, T, Modifiers...>& left,
               const ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = arithmetic_checking::remove_optional_t<T>;
  static_assert(arithmetic_checking::Arithmetic<LeftBaseType>,
                arithmetic_checking::arithmetic_error_message);

  auto left_expr = to_expr(left);
  return ArithmeticExpr<decltype(left_expr), ArithmeticExpr<Left, Right>>(std::move(left_expr), "/",
                                                                          right_expr);
}

/// @brief Division operator for ArithmeticExpr with value
template <SqlExpr Left, SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(const ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = val(std::forward<ValueT>(value));
  return ArithmeticExpr<ArithmeticExpr<Left, Right>, decltype(value_expr)>(left_expr, "/",
                                                                           std::move(value_expr));
}

/// @brief Division operator for value with ArithmeticExpr
template <typename ValueT, SqlExpr Left, SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(ValueT&& value, const query::ArithmeticExpr<Left, Right>& right_expr) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(value_expr), decltype(right_expr)>(
      std::move(value_expr), "/", std::move(right_expr));
}

}  // namespace relx::query

// Arithmetic operators need to be in the schema namespace for ADL to find them
namespace relx::schema {

/// @brief Addition operator for columns with type checking
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator+(const column<TableT1, Name1, T1, Modifiers1...>& left,
               const column<TableT2, Name2, T2, Modifiers2...>& right) {
  // Extract base types, removing optional wrapper if present
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T2>;

  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "+",
                                                                          std::move(right_expr));
}

/// @brief Addition operator for column with value
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(const column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(
      query::arithmetic_checking::Arithmetic<ValueT>,
      "Arithmetic operations require numeric values. Cannot add non-numeric values to columns.");

  auto left_expr = query::to_expr(left);
  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "+",
                                                                          std::move(value_expr));
}

/// @brief Addition operator for value with column (reversed)
template <typename ValueT, typename TableT, FixedString Name, typename T, typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(ValueT&& value, const column<TableT, Name, T, Modifiers...>& right) {
  return right + std::forward<ValueT>(value);
}

/// @brief Subtraction operator for columns
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator-(const column<TableT1, Name1, T1, Modifiers1...>& left,
               const column<TableT2, Name2, T2, Modifiers2...>& right) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T2>;

  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "-",
                                                                          std::move(right_expr));
}

/// @brief Subtraction operator for column with value
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(const column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values. Cannot subtract non-numeric values "
                "from columns.");

  auto left_expr = query::to_expr(left);
  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "-",
                                                                          std::move(value_expr));
}

/// @brief Subtraction operator for value with column (reversed)
template <typename ValueT, typename TableT, FixedString Name, typename T, typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(ValueT&& value, const column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<decltype(value_expr), decltype(right_expr)>(
      std::move(value_expr), "-", std::move(right_expr));
}

/// @brief Multiplication operator for columns
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator*(const column<TableT1, Name1, T1, Modifiers1...>& left,
               const column<TableT2, Name2, T2, Modifiers2...>& right) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T2>;

  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "*",
                                                                          std::move(right_expr));
}

/// @brief Multiplication operator for column with value
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(const column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values. Cannot multiply columns by "
                "non-numeric values.");

  auto left_expr = query::to_expr(left);
  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "*",
                                                                          std::move(value_expr));
}

/// @brief Multiplication operator for value with column (reversed)
template <typename ValueT, typename TableT, FixedString Name, typename T, typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(ValueT&& value, const column<TableT, Name, T, Modifiers...>& right) {
  return right * std::forward<ValueT>(value);
}

/// @brief Division operator for columns
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator/(const column<TableT1, Name1, T1, Modifiers1...>& left,
               const column<TableT2, Name2, T2, Modifiers2...>& right) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T1>;
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T2>;

  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<decltype(left_expr), decltype(right_expr)>(std::move(left_expr), "/",
                                                                          std::move(right_expr));
}

/// @brief Division operator for column with value
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(const column<TableT, Name, T, Modifiers...>& left, ValueT&& value) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(
      query::arithmetic_checking::Arithmetic<ValueT>,
      "Arithmetic operations require numeric values. Cannot divide columns by non-numeric values.");

  auto left_expr = query::to_expr(left);
  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(left_expr), decltype(value_expr)>(std::move(left_expr), "/",
                                                                          std::move(value_expr));
}

/// @brief Division operator for value with column (reversed)
template <typename ValueT, typename TableT, FixedString Name, typename T, typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(ValueT&& value, const column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<decltype(value_expr), decltype(right_expr)>(
      std::move(value_expr), "/", std::move(right_expr));
}

// Arithmetic operators for ArithmeticExpr to support chaining (in schema namespace for ADL)

/// @brief Addition operator for ArithmeticExpr with column
template <query::SqlExpr Left, query::SqlExpr Right, typename TableT, FixedString Name, typename T,
          typename... Modifiers>
auto operator+(const query::ArithmeticExpr<Left, Right>& left_expr,
               const column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(right_expr)>(
      left_expr, "+", std::move(right_expr));
}

/// @brief Addition operator for column with ArithmeticExpr
template <typename TableT, FixedString Name, typename T, typename... Modifiers, query::SqlExpr Left,
          query::SqlExpr Right>
auto operator+(const column<TableT, Name, T, Modifiers...>& left,
               const query::ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  return query::ArithmeticExpr<decltype(left_expr), query::ArithmeticExpr<Left, Right>>(
      std::move(left_expr), "+", right_expr);
}

/// @brief Addition operator for ArithmeticExpr with value
template <query::SqlExpr Left, query::SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(const query::ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(value_expr)>(
      left_expr, "+", std::move(value_expr));
}

/// @brief Addition operator for value with ArithmeticExpr
template <typename ValueT, query::SqlExpr Left, query::SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator+(ValueT&& value, const query::ArithmeticExpr<Left, Right>& right_expr) {
  return right_expr + std::forward<ValueT>(value);
}

/// @brief Subtraction operator for ArithmeticExpr with column
template <query::SqlExpr Left, query::SqlExpr Right, typename TableT, FixedString Name, typename T,
          typename... Modifiers>
auto operator-(const query::ArithmeticExpr<Left, Right>& left_expr,
               const column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(right_expr)>(
      left_expr, "-", std::move(right_expr));
}

/// @brief Subtraction operator for column with ArithmeticExpr
template <typename TableT, FixedString Name, typename T, typename... Modifiers, query::SqlExpr Left,
          query::SqlExpr Right>
auto operator-(const column<TableT, Name, T, Modifiers...>& left,
               const query::ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  return query::ArithmeticExpr<decltype(left_expr), query::ArithmeticExpr<Left, Right>>(
      std::move(left_expr), "-", right_expr);
}

/// @brief Subtraction operator for ArithmeticExpr with value
template <query::SqlExpr Left, query::SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(const query::ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(value_expr)>(
      left_expr, "-", std::move(value_expr));
}

/// @brief Subtraction operator for value with ArithmeticExpr
template <typename ValueT, query::SqlExpr Left, query::SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator-(ValueT&& value, const query::ArithmeticExpr<Left, Right>& right_expr) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(value_expr), query::ArithmeticExpr<Left, Right>>(
      std::move(value_expr), "-", right_expr);
}

/// @brief Multiplication operator for ArithmeticExpr with column
template <query::SqlExpr Left, query::SqlExpr Right, typename TableT, FixedString Name, typename T,
          typename... Modifiers>
auto operator*(const query::ArithmeticExpr<Left, Right>& left_expr,
               const column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(right_expr)>(
      left_expr, "*", std::move(right_expr));
}

/// @brief Multiplication operator for column with ArithmeticExpr
template <typename TableT, FixedString Name, typename T, typename... Modifiers, query::SqlExpr Left,
          query::SqlExpr Right>
auto operator*(const column<TableT, Name, T, Modifiers...>& left,
               const query::ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  return query::ArithmeticExpr<decltype(left_expr), query::ArithmeticExpr<Left, Right>>(
      std::move(left_expr), "*", right_expr);
}

/// @brief Multiplication operator for ArithmeticExpr with value
template <query::SqlExpr Left, query::SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(const query::ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(value_expr)>(
      left_expr, "*", std::move(value_expr));
}

/// @brief Multiplication operator for value with ArithmeticExpr
template <typename ValueT, query::SqlExpr Left, query::SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator*(ValueT&& value, const query::ArithmeticExpr<Left, Right>& right_expr) {
  return right_expr * std::forward<ValueT>(value);
}

/// @brief Division operator for ArithmeticExpr with column
template <query::SqlExpr Left, query::SqlExpr Right, typename TableT, FixedString Name, typename T,
          typename... Modifiers>
auto operator/(const query::ArithmeticExpr<Left, Right>& left_expr,
               const column<TableT, Name, T, Modifiers...>& right) {
  using RightBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<RightBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto right_expr = query::to_expr(right);
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(right_expr)>(
      left_expr, "/", std::move(right_expr));
}

/// @brief Division operator for column with ArithmeticExpr
template <typename TableT, FixedString Name, typename T, typename... Modifiers, query::SqlExpr Left,
          query::SqlExpr Right>
auto operator/(const column<TableT, Name, T, Modifiers...>& left,
               const query::ArithmeticExpr<Left, Right>& right_expr) {
  using LeftBaseType = query::arithmetic_checking::remove_optional_t<T>;
  static_assert(query::arithmetic_checking::Arithmetic<LeftBaseType>,
                query::arithmetic_checking::arithmetic_error_message);

  auto left_expr = query::to_expr(left);
  return query::ArithmeticExpr<decltype(left_expr), query::ArithmeticExpr<Left, Right>>(
      std::move(left_expr), "/", right_expr);
}

/// @brief Division operator for ArithmeticExpr with value
template <query::SqlExpr Left, query::SqlExpr Right, typename ValueT>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(const query::ArithmeticExpr<Left, Right>& left_expr, ValueT&& value) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<query::ArithmeticExpr<Left, Right>, decltype(value_expr)>(
      left_expr, "/", std::move(value_expr));
}

/// @brief Division operator for value with ArithmeticExpr
template <typename ValueT, query::SqlExpr Left, query::SqlExpr Right>
  requires std::is_arithmetic_v<std::remove_cvref_t<ValueT>>
auto operator/(ValueT&& value, const query::ArithmeticExpr<Left, Right>& right_expr) {
  static_assert(query::arithmetic_checking::Arithmetic<ValueT>,
                "Arithmetic operations require numeric values.");

  auto value_expr = query::val(std::forward<ValueT>(value));
  return query::ArithmeticExpr<decltype(value_expr), decltype(right_expr)>(
      std::move(value_expr), "/", std::move(right_expr));
}

}  // namespace relx::schema