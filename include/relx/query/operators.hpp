#pragma once

#include "../query/condition.hpp"
#include "../query/date_concepts.hpp"
#include "../query/function.hpp"
#include "../query/meta.hpp"
#include "../query/schema_adapter.hpp"
#include "../query/value.hpp"
#include "../schema/column.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace relx {
/// @brief Type compatibility utilities for column comparisons
namespace type_checking {

/// @brief Helper to extract the underlying type from optional
template <typename T>
struct remove_optional {
  using type = T;
  static constexpr bool is_optional = false;
};

template <typename T>
struct remove_optional<std::optional<T>> {
  using type = T;
  static constexpr bool is_optional = true;
};

template <typename T>
using remove_optional_t = typename remove_optional<T>::type;

/// @brief Check if two types are string-compatible
template <typename T1, typename T2>
concept StringCompatible = (std::same_as<std::remove_cvref_t<T1>, std::string> ||
                            std::same_as<std::remove_cvref_t<T1>, std::string_view> ||
                            std::same_as<std::remove_cvref_t<T1>, const char*>) &&
                           (std::same_as<std::remove_cvref_t<T2>, std::string> ||
                            std::same_as<std::remove_cvref_t<T2>, std::string_view> ||
                            std::same_as<std::remove_cvref_t<T2>, const char*> ||
                            std::convertible_to<std::remove_cvref_t<T2>, std::string> ||
                            std::convertible_to<std::remove_cvref_t<T2>, std::string_view>);

/// @brief Check if optional types are compatible with each other or their underlying types
template <typename ColumnType, typename ValueType>
concept OptionalCompatible =
    // Optional to non-optional: optional<T> with T
    (remove_optional<std::remove_cvref_t<ColumnType>>::is_optional &&
     std::same_as<remove_optional_t<std::remove_cvref_t<ColumnType>>,
                  std::remove_cvref_t<ValueType>>) ||
    // Non-optional to optional: T with optional<T>
    (remove_optional<std::remove_cvref_t<ValueType>>::is_optional &&
     std::same_as<std::remove_cvref_t<ColumnType>,
                  remove_optional_t<std::remove_cvref_t<ValueType>>>) ||
    // Optional string compatibility: optional<string> with string-like types
    (remove_optional<std::remove_cvref_t<ColumnType>>::is_optional &&
     StringCompatible<remove_optional_t<std::remove_cvref_t<ColumnType>>, ValueType>) ||
    // String-like types with optional<string>
    (remove_optional<std::remove_cvref_t<ValueType>>::is_optional &&
     StringCompatible<ColumnType, remove_optional_t<std::remove_cvref_t<ValueType>>>) ||
    // Both optional: optional<T> with optional<U> where T and U are compatible
    (remove_optional<std::remove_cvref_t<ColumnType>>::is_optional &&
     remove_optional<std::remove_cvref_t<ValueType>>::is_optional &&
     (std::same_as<remove_optional_t<std::remove_cvref_t<ColumnType>>,
                   remove_optional_t<std::remove_cvref_t<ValueType>>> ||
      StringCompatible<remove_optional_t<std::remove_cvref_t<ColumnType>>,
                       remove_optional_t<std::remove_cvref_t<ValueType>>>));

/// @brief Check if column type is compatible with value type
template <typename ColumnType, typename ValueType>
concept TypeCompatible =
    // Exact match
    std::same_as<std::remove_cvref_t<ColumnType>, std::remove_cvref_t<ValueType>> ||
    // String compatibility
    StringCompatible<ColumnType, ValueType> ||
    // Optional compatibility
    OptionalCompatible<ColumnType, ValueType>;

/// @brief Constant error message for type mismatches
inline constexpr std::string_view type_error_message =
    "Column type and value type are not compatible. "
    "Column types must match the value types being compared. "
    "For string columns, you can use std::string, std::string_view, or const char*. "
    "For optional columns, you can compare with the underlying type or another optional. "
    "For numeric columns, types must match exactly (use explicit casts if needed).";
}  // namespace type_checking

namespace schema {

// Binary comparison operators for columns

// Equality comparison with direct values
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueType>
auto operator==(const column<TableT, Name, T, Modifiers...>& col, const ValueType& value) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col == query::val(value);
}

// Inequality comparison with direct values
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueType>
auto operator!=(const column<TableT, Name, T, Modifiers...>& col, const ValueType& value) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col != query::val(value);
}

// Greater than comparison with direct values
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueType>
auto operator>(const column<TableT, Name, T, Modifiers...>& col, const ValueType& value) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col > query::val(value);
}

// Less than comparison with direct values
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueType>
auto operator<(const column<TableT, Name, T, Modifiers...>& col, const ValueType& value) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col < query::val(value);
}

// Greater than or equal comparison with direct values
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueType>
auto operator>=(const column<TableT, Name, T, Modifiers...>& col, const ValueType& value) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col >= query::val(value);
}

// Less than or equal comparison with direct values
template <typename TableT, FixedString Name, typename T, typename... Modifiers, typename ValueType>
auto operator<=(const column<TableT, Name, T, Modifiers...>& col, const ValueType& value) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col <= query::val(value);
}

// Column to column equality comparison
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator==(const column<TableT1, Name1, T1, Modifiers1...>& col1,
                const column<TableT2, Name2, T2, Modifiers2...>& col2) {
  static_assert(type_checking::TypeCompatible<T1, T2>,
                "Column types in comparisons (especially JOIN conditions) must be compatible. "
                "For example, you cannot join an int column with a string column. "
                "Both columns must have the same type or be string-compatible types.");
  auto col1_expr = query::to_expr(col1);
  auto col2_expr = query::to_expr(col2);
  return col1_expr == col2_expr;
}

// Column to column inequality comparison
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator!=(const column<TableT1, Name1, T1, Modifiers1...>& col1,
                const column<TableT2, Name2, T2, Modifiers2...>& col2) {
  static_assert(type_checking::TypeCompatible<T1, T2>,
                "Column types in comparisons must be compatible. "
                "Both columns must have the same type or be string-compatible types.");
  auto col1_expr = query::to_expr(col1);
  auto col2_expr = query::to_expr(col2);
  return col1_expr != col2_expr;
}

// Column to column greater than comparison
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator>(const column<TableT1, Name1, T1, Modifiers1...>& col1,
               const column<TableT2, Name2, T2, Modifiers2...>& col2) {
  static_assert(type_checking::TypeCompatible<T1, T2>,
                "Column types in comparisons must be compatible. "
                "Both columns must have the same type or be string-compatible types.");
  auto col1_expr = query::to_expr(col1);
  auto col2_expr = query::to_expr(col2);
  return col1_expr > col2_expr;
}

// Column to column less than comparison
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator<(const column<TableT1, Name1, T1, Modifiers1...>& col1,
               const column<TableT2, Name2, T2, Modifiers2...>& col2) {
  static_assert(type_checking::TypeCompatible<T1, T2>,
                "Column types in comparisons must be compatible. "
                "Both columns must have the same type or be string-compatible types.");
  auto col1_expr = query::to_expr(col1);
  auto col2_expr = query::to_expr(col2);
  return col1_expr < col2_expr;
}

// Column to column greater than or equal comparison
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator>=(const column<TableT1, Name1, T1, Modifiers1...>& col1,
                const column<TableT2, Name2, T2, Modifiers2...>& col2) {
  static_assert(type_checking::TypeCompatible<T1, T2>,
                "Column types in comparisons must be compatible. "
                "Both columns must have the same type or be string-compatible types.");
  auto col1_expr = query::to_expr(col1);
  auto col2_expr = query::to_expr(col2);
  return col1_expr >= col2_expr;
}

// Column to column less than or equal comparison
template <typename TableT1, FixedString Name1, typename T1, typename... Modifiers1,
          typename TableT2, FixedString Name2, typename T2, typename... Modifiers2>
auto operator<=(const column<TableT1, Name1, T1, Modifiers1...>& col1,
                const column<TableT2, Name2, T2, Modifiers2...>& col2) {
  static_assert(type_checking::TypeCompatible<T1, T2>,
                "Column types in comparisons must be compatible. "
                "Both columns must have the same type or be string-compatible types.");
  auto col1_expr = query::to_expr(col1);
  auto col2_expr = query::to_expr(col2);
  return col1_expr <= col2_expr;
}

// Specialized operators for common types

// Boolean negation operator
template <typename TableT, FixedString Name, typename... Modifiers>
auto operator!(const column<TableT, Name, bool, Modifiers...>& col) {
  auto col_expr = query::to_expr(col);
  // Using the logical NOT operator
  return !col_expr;
}

// Logical AND operator between a column and a query::SqlExpr
template <typename TableT, FixedString Name, typename... Modifiers, query::SqlExpr Expr>
auto operator&&(const column<TableT, Name, bool, Modifiers...>& col, const Expr& expr) {
  auto col_expr = query::to_expr(col);
  return col_expr && expr;
}

// Logical AND operator between a query::SqlExpr and a column (reverse)
template <query::SqlExpr Expr, typename TableT, FixedString Name, typename... Modifiers>
auto operator&&(const Expr& expr, const column<TableT, Name, bool, Modifiers...>& col) {
  auto col_expr = query::to_expr(col);
  return expr && col_expr;
}

// Logical OR operator between a column and a query::SqlExpr
template <typename TableT, FixedString Name, typename... Modifiers, query::SqlExpr Expr>
auto operator||(const column<TableT, Name, bool, Modifiers...>& col, const Expr& expr) {
  auto col_expr = query::to_expr(col);
  return col_expr || expr;
}

// Logical OR operator between a query::SqlExpr and a column (reverse)
template <query::SqlExpr Expr, typename TableT, FixedString Name, typename... Modifiers>
auto operator||(const Expr& expr, const column<TableT, Name, bool, Modifiers...>& col) {
  auto col_expr = query::to_expr(col);
  return expr || col_expr;
}

// Symmetrical operators (value on left, column on right)

// Equality comparison with direct values (reversed)
template <typename ValueType, typename TableT, FixedString Name, typename T, typename... Modifiers>
auto operator==(const ValueType& value, const column<TableT, Name, T, Modifiers...>& col) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col == value;
}

// Inequality comparison with direct values (reversed)
template <typename ValueType, typename TableT, FixedString Name, typename T, typename... Modifiers>
auto operator!=(const ValueType& value, const column<TableT, Name, T, Modifiers...>& col) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col != value;
}

// Greater than comparison with direct values (reversed)
template <typename ValueType, typename TableT, FixedString Name, typename T, typename... Modifiers>
auto operator>(const ValueType& value, const column<TableT, Name, T, Modifiers...>& col) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col < value;
}

// Less than comparison with direct values (reversed)
template <typename ValueType, typename TableT, FixedString Name, typename T, typename... Modifiers>
auto operator<(const ValueType& value, const column<TableT, Name, T, Modifiers...>& col) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col > value;
}

// Greater than or equal comparison with direct values (reversed)
template <typename ValueType, typename TableT, FixedString Name, typename T, typename... Modifiers>
auto operator>=(const ValueType& value, const column<TableT, Name, T, Modifiers...>& col) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col <= value;
}

// Less than or equal comparison with direct values (reversed)
template <typename ValueType, typename TableT, FixedString Name, typename T, typename... Modifiers>
auto operator<=(const ValueType& value, const column<TableT, Name, T, Modifiers...>& col) {
  static_assert(type_checking::TypeCompatible<T, ValueType>, type_checking::type_error_message);
  return col >= value;
}

// Boolean operators for columns with conditions

// Logical AND between column and any condition
template <typename TableT, FixedString Name, typename... Modifiers, typename Cond>
  requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator&&(const column<TableT, Name, bool, Modifiers...>& col, const Cond& cond) {
  auto col_expr = query::to_expr(col);
  if constexpr (query::SqlExpr<Cond>) {
    return col_expr && cond;
  } else {
    auto val_expr = query::val(cond);
    return col_expr && val_expr;
  }
}

// Logical OR between column and any condition
template <typename TableT, FixedString Name, typename... Modifiers, typename Cond>
  requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator||(const column<TableT, Name, bool, Modifiers...>& col, const Cond& cond) {
  auto col_expr = query::to_expr(col);
  if constexpr (query::SqlExpr<Cond>) {
    return col_expr || cond;
  } else {
    auto val_expr = query::val(cond);
    return col_expr || val_expr;
  }
}

// Logical AND between condition and column (reversed)
template <typename Cond, typename TableT, FixedString Name, typename... Modifiers>
  requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator&&(const Cond& cond, const column<TableT, Name, bool, Modifiers...>& col) {
  auto col_expr = query::to_expr(col);
  if constexpr (query::SqlExpr<Cond>) {
    return cond && col_expr;
  } else {
    auto val_expr = query::val(cond);
    return val_expr && col_expr;
  }
}

// Logical OR between condition and column (reversed)
template <typename Cond, typename TableT, FixedString Name, typename... Modifiers>
  requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator||(const Cond& cond, const column<TableT, Name, bool, Modifiers...>& col) {
  auto col_expr = query::to_expr(col);
  if constexpr (query::SqlExpr<Cond>) {
    return cond || col_expr;
  } else {
    auto val_expr = query::val(cond);
    return val_expr || col_expr;
  }
}

}  // namespace schema

namespace query {

// Column to SQL Value comparisons (for _sql literals)

// Define a helper meta trait for checking specializations
namespace meta {
template <typename T, template <typename...> class Template>
struct is_specialization : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> class Template>
constexpr bool is_specialization_v = is_specialization<T, Template>::value;
}  // namespace meta

// Equality comparison with Value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
auto operator==(const schema::column<TableT, Name, T, Modifiers...>& col,
                const Value<ValueT>& value) {
  auto col_expr = to_expr(col);
  return col_expr == value;
}

// Inequality comparison with Value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
auto operator!=(const schema::column<TableT, Name, T, Modifiers...>& col,
                const Value<ValueT>& value) {
  auto col_expr = to_expr(col);
  return col_expr != value;
}

// Greater than comparison with Value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
auto operator>(const schema::column<TableT, Name, T, Modifiers...>& col,
               const Value<ValueT>& value) {
  auto col_expr = to_expr(col);
  return col_expr > value;
}

// Less than comparison with Value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
auto operator<(const schema::column<TableT, Name, T, Modifiers...>& col,
               const Value<ValueT>& value) {
  auto col_expr = to_expr(col);
  return col_expr < value;
}

// Greater than or equal comparison with Value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
auto operator>=(const schema::column<TableT, Name, T, Modifiers...>& col,
                const Value<ValueT>& value) {
  auto col_expr = to_expr(col);
  return col_expr >= value;
}

// Less than or equal comparison with Value
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ValueT>
auto operator<=(const schema::column<TableT, Name, T, Modifiers...>& col,
                const Value<ValueT>& value) {
  auto col_expr = to_expr(col);
  return col_expr <= value;
}

// Column comparison with direct numeric literals
// These overloads allow for expressions like col > 42 without needing query::val(42)

// Equality comparison with numeric literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator==(const schema::column<TableT, Name, T, Modifiers...>& col, LiteralT&& literal) {
  auto col_expr = to_expr(col);
  auto val_expr = val(std::forward<LiteralT>(literal));
  return col_expr == val_expr;
}

// Inequality comparison with numeric literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator!=(const schema::column<TableT, Name, T, Modifiers...>& col, LiteralT&& literal) {
  auto col_expr = to_expr(col);
  auto val_expr = val(std::forward<LiteralT>(literal));
  return col_expr != val_expr;
}

// Greater than comparison with numeric literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>(const schema::column<TableT, Name, T, Modifiers...>& col, LiteralT&& literal) {
  auto col_expr = to_expr(col);
  auto val_expr = val(std::forward<LiteralT>(literal));
  return col_expr > val_expr;
}

// Less than comparison with numeric literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<(const schema::column<TableT, Name, T, Modifiers...>& col, LiteralT&& literal) {
  auto col_expr = to_expr(col);
  auto val_expr = val(std::forward<LiteralT>(literal));
  return col_expr < val_expr;
}

// Greater than or equal comparison with numeric literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>=(const schema::column<TableT, Name, T, Modifiers...>& col, LiteralT&& literal) {
  auto col_expr = to_expr(col);
  auto val_expr = val(std::forward<LiteralT>(literal));
  return col_expr >= val_expr;
}

// Less than or equal comparison with numeric literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<=(const schema::column<TableT, Name, T, Modifiers...>& col, LiteralT&& literal) {
  auto col_expr = to_expr(col);
  auto val_expr = val(std::forward<LiteralT>(literal));
  return col_expr <= val_expr;
}

// Symmetrical operators for numeric literals (literal on left, column on right)

// Equality comparison with numeric literal (reversed)
template <typename LiteralT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator==(LiteralT&& literal, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col == std::forward<LiteralT>(literal);
}

// Inequality comparison with numeric literal (reversed)
template <typename LiteralT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator!=(LiteralT&& literal, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col != std::forward<LiteralT>(literal);
}

// Greater than comparison with numeric literal (reversed)
template <typename LiteralT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>(LiteralT&& literal, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col < std::forward<LiteralT>(literal);
}

// Less than comparison with numeric literal (reversed)
template <typename LiteralT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<(LiteralT&& literal, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col > std::forward<LiteralT>(literal);
}

// Greater than or equal comparison with numeric literal (reversed)
template <typename LiteralT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>=(LiteralT&& literal, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col <= std::forward<LiteralT>(literal);
}

// Less than or equal comparison with numeric literal (reversed)
template <typename LiteralT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<=(LiteralT&& literal, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col >= std::forward<LiteralT>(literal);
}

// String literal comparison
// These allow for using string literals directly without query::val

// Equality comparison with string literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto operator==(const schema::column<TableT, Name, T, Modifiers...>& col, const char* str) {
  auto col_expr = to_expr(col);
  auto val_expr = val(str);
  return col_expr == val_expr;
}

// Inequality comparison with string literal
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto operator!=(const schema::column<TableT, Name, T, Modifiers...>& col, const char* str) {
  auto col_expr = to_expr(col);
  auto val_expr = val(str);
  return col_expr != val_expr;
}

// String literal comparison (reversed)
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto operator==(const char* str, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col == str;
}

template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto operator!=(const char* str, const schema::column<TableT, Name, T, Modifiers...>& col) {
  return col != str;
}

// Additional column operation overloads to avoid to_expr wrappers

// LIKE operator for columns
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto like(const schema::column<TableT, Name, T, Modifiers...>& col, std::string pattern) {
  auto col_expr = to_expr(col);
  return like(col_expr, std::move(pattern));
}

// IS NULL operator for columns
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto is_null(const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return is_null(col_expr);
}

// IS NOT NULL operator for columns
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto is_not_null(const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return is_not_null(col_expr);
}

// BETWEEN operator for columns
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
auto between(const schema::column<TableT, Name, T, Modifiers...>& col, std::string lower,
             std::string upper) {
  auto col_expr = to_expr(col);
  return between(col_expr, std::move(lower), std::move(upper));
}

// Column support for case expressions

// When with a column condition
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename ResultT>
auto when(const schema::column<TableT, Name, T, Modifiers...>& condition,
          const query::Value<ResultT>& result) {
  auto col_expr = to_expr(condition);
  return when(col_expr, result);
}

// When with a column result
template <typename CondT, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
auto when(const CondT& condition, const schema::column<TableT, Name, T, Modifiers...>& result) {
  auto result_expr = to_expr(result);
  return when(condition, result_expr);
}

// Else with a column result
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
// NOLINTNEXTLINE(readability-identifier-naming)
auto else_(const schema::column<TableT, Name, T, Modifiers...>& result) {
  auto result_expr = to_expr(result);
  return else_(result_expr);
}

// Column support for select expressions
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers,
          typename... Args>
auto select_expr(const schema::column<TableT, Name, T, Modifiers...>& col, Args&&... args) {
  auto col_expr = to_expr(col);
  return select_expr(col_expr, std::forward<Args>(args)...);
}

// Adapter to literal comparison operators

// SchemaColumnAdapter comparison with direct literals
template <typename Column, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator==(const SchemaColumnAdapter<Column>& col, LiteralT&& literal) {
  return col == val(std::forward<LiteralT>(literal));
}

template <typename Column, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator!=(const SchemaColumnAdapter<Column>& col, LiteralT&& literal) {
  return col != val(std::forward<LiteralT>(literal));
}

template <typename Column, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>(const SchemaColumnAdapter<Column>& col, LiteralT&& literal) {
  return col > val(std::forward<LiteralT>(literal));
}

template <typename Column, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<(const SchemaColumnAdapter<Column>& col, LiteralT&& literal) {
  return col < val(std::forward<LiteralT>(literal));
}

template <typename Column, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>=(const SchemaColumnAdapter<Column>& col, LiteralT&& literal) {
  return col >= val(std::forward<LiteralT>(literal));
}

template <typename Column, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<=(const SchemaColumnAdapter<Column>& col, LiteralT&& literal) {
  return col <= val(std::forward<LiteralT>(literal));
}

// Reversed operators
template <typename LiteralT, typename Column>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator==(LiteralT&& literal, const SchemaColumnAdapter<Column>& col) {
  return col == std::forward<LiteralT>(literal);
}

template <typename LiteralT, typename Column>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator!=(LiteralT&& literal, const SchemaColumnAdapter<Column>& col) {
  return col != std::forward<LiteralT>(literal);
}

template <typename LiteralT, typename Column>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>(LiteralT&& literal, const SchemaColumnAdapter<Column>& col) {
  return col < std::forward<LiteralT>(literal);
}

template <typename LiteralT, typename Column>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<(LiteralT&& literal, const SchemaColumnAdapter<Column>& col) {
  return col > std::forward<LiteralT>(literal);
}

template <typename LiteralT, typename Column>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>=(LiteralT&& literal, const SchemaColumnAdapter<Column>& col) {
  return col <= std::forward<LiteralT>(literal);
}

template <typename LiteralT, typename Column>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> &&
           (!std::is_same_v<LiteralT, bool>) &&
           (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<=(LiteralT&& literal, const SchemaColumnAdapter<Column>& col) {
  return col >= std::forward<LiteralT>(literal);
}

// String literal comparisons for SchemaColumnAdapter
template <typename Column>
auto operator==(const SchemaColumnAdapter<Column>& col, const char* str) {
  return col == val(str);
}

template <typename Column>
auto operator!=(const SchemaColumnAdapter<Column>& col, const char* str) {
  return col != val(str);
}

template <typename Column>
auto operator==(const char* str, const SchemaColumnAdapter<Column>& col) {
  return col == str;
}

template <typename Column>
auto operator!=(const char* str, const SchemaColumnAdapter<Column>& col) {
  return col != str;
}

// Additional column operation overloads to avoid to_expr wrappers

// Operators for AliasedColumn to work with literals

// For AliasedColumn == literals
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(const AliasedColumn<Expr>& col, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<AliasedColumn<Expr>, decltype(val_expr)>(col, "=", val_expr);
}

// For literals == AliasedColumn
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(LiteralT&& literal, const AliasedColumn<Expr>& col) {
  return col == std::forward<LiteralT>(literal);
}

// Special case for string literals with AliasedColumn
template <SqlExpr Expr>
auto operator==(const AliasedColumn<Expr>& col, const char* str) {
  auto str_val = val(str);
  return BinaryCondition<AliasedColumn<Expr>, decltype(str_val)>(col, "=", str_val);
}

template <SqlExpr Expr>
auto operator==(const char* str, const AliasedColumn<Expr>& col) {
  return col == str;
}

// For AliasedColumn != literals
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(const AliasedColumn<Expr>& col, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<AliasedColumn<Expr>, decltype(val_expr)>(col, "!=", val_expr);
}

// For literals != AliasedColumn
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(LiteralT&& literal, const AliasedColumn<Expr>& col) {
  return col != std::forward<LiteralT>(literal);
}

// Special case for string literals with AliasedColumn
template <SqlExpr Expr>
auto operator!=(const AliasedColumn<Expr>& col, const char* str) {
  auto str_val = val(str);
  return BinaryCondition<AliasedColumn<Expr>, decltype(str_val)>(col, "!=", str_val);
}

template <SqlExpr Expr>
auto operator!=(const char* str, const AliasedColumn<Expr>& col) {
  return col != str;
}

// For other comparison operators (>, <, >=, <=)
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>(const AliasedColumn<Expr>& col, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<AliasedColumn<Expr>, decltype(val_expr)>(col, ">", val_expr);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<(const AliasedColumn<Expr>& col, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<AliasedColumn<Expr>, decltype(val_expr)>(col, "<", val_expr);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>=(const AliasedColumn<Expr>& col, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<AliasedColumn<Expr>, decltype(val_expr)>(col, ">=", val_expr);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<=(const AliasedColumn<Expr>& col, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<AliasedColumn<Expr>, decltype(val_expr)>(col, "<=", val_expr);
}

// Reversed comparison operators with literals
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>(LiteralT&& literal, const AliasedColumn<Expr>& col) {
  return col < std::forward<LiteralT>(literal);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<(LiteralT&& literal, const AliasedColumn<Expr>& col) {
  return col > std::forward<LiteralT>(literal);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>=(LiteralT&& literal, const AliasedColumn<Expr>& col) {
  return col <= std::forward<LiteralT>(literal);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<=(LiteralT&& literal, const AliasedColumn<Expr>& col) {
  return col >= std::forward<LiteralT>(literal);
}

// Operators for FunctionExpr to work with literals

// For FunctionExpr == literals
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(const FunctionExpr<Expr>& func, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<FunctionExpr<Expr>, decltype(val_expr)>(func, "=", val_expr);
}

// For literals == FunctionExpr
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(LiteralT&& literal, const FunctionExpr<Expr>& func) {
  return func == std::forward<LiteralT>(literal);
}

// Special case for string literals with FunctionExpr
template <SqlExpr Expr>
auto operator==(const FunctionExpr<Expr>& func, const char* str) {
  auto str_val = val(str);
  return BinaryCondition<FunctionExpr<Expr>, decltype(str_val)>(func, "=", str_val);
}

template <SqlExpr Expr>
auto operator==(const char* str, const FunctionExpr<Expr>& func) {
  return func == str;
}

// For FunctionExpr != literals
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(const FunctionExpr<Expr>& func, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<FunctionExpr<Expr>, decltype(val_expr)>(func, "!=", val_expr);
}

// For literals != FunctionExpr
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(LiteralT&& literal, const FunctionExpr<Expr>& func) {
  return func != std::forward<LiteralT>(literal);
}

// Special case for string literals with FunctionExpr
template <SqlExpr Expr>
auto operator!=(const FunctionExpr<Expr>& func, const char* str) {
  auto str_val = val(str);
  return BinaryCondition<FunctionExpr<Expr>, decltype(str_val)>(func, "!=", str_val);
}

template <SqlExpr Expr>
auto operator!=(const char* str, const FunctionExpr<Expr>& func) {
  return func != str;
}

// For other comparison operators (>, <, >=, <=)
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>(const FunctionExpr<Expr>& func, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<FunctionExpr<Expr>, decltype(val_expr)>(func, ">", val_expr);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<(const FunctionExpr<Expr>& func, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<FunctionExpr<Expr>, decltype(val_expr)>(func, "<", val_expr);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>=(const FunctionExpr<Expr>& func, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<FunctionExpr<Expr>, decltype(val_expr)>(func, ">=", val_expr);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<=(const FunctionExpr<Expr>& func, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<FunctionExpr<Expr>, decltype(val_expr)>(func, "<=", val_expr);
}

// Reversed comparison operators with literals
template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>(LiteralT&& literal, const FunctionExpr<Expr>& func) {
  return func < std::forward<LiteralT>(literal);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<(LiteralT&& literal, const FunctionExpr<Expr>& func) {
  return func > std::forward<LiteralT>(literal);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>=(LiteralT&& literal, const FunctionExpr<Expr>& func) {
  return func <= std::forward<LiteralT>(literal);
}

template <SqlExpr Expr, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<=(LiteralT&& literal, const FunctionExpr<Expr>& func) {
  return func >= std::forward<LiteralT>(literal);
}

// Operators for CoalesceExpr to work with literals

// For CoalesceExpr == literals
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(const CoalesceExpr<First, Second, Rest...>& coalesce, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(val_expr)>(coalesce, "=",
                                                                                   val_expr);
}

// For literals == CoalesceExpr
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(LiteralT&& literal, const CoalesceExpr<First, Second, Rest...>& coalesce) {
  return coalesce == std::forward<LiteralT>(literal);
}

// Special case for string literals with CoalesceExpr
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto operator==(const CoalesceExpr<First, Second, Rest...>& coalesce, const char* str) {
  auto str_val = val(str);
  return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(str_val)>(coalesce, "=",
                                                                                  str_val);
}

template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto operator==(const char* str, const CoalesceExpr<First, Second, Rest...>& coalesce) {
  return coalesce == str;
}

// For CoalesceExpr != literals
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(const CoalesceExpr<First, Second, Rest...>& coalesce, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(val_expr)>(coalesce,
                                                                                   "!=", val_expr);
}

// For literals != CoalesceExpr
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest, typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(LiteralT&& literal, const CoalesceExpr<First, Second, Rest...>& coalesce) {
  return coalesce != std::forward<LiteralT>(literal);
}

// Special case for string literals with CoalesceExpr
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto operator!=(const CoalesceExpr<First, Second, Rest...>& coalesce, const char* str) {
  auto str_val = val(str);
  return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(str_val)>(coalesce,
                                                                                  "!=", str_val);
}

template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto operator!=(const char* str, const CoalesceExpr<First, Second, Rest...>& coalesce) {
  return coalesce != str;
}

// Operators for CountAllExpr to work with literals
template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(const CountAllExpr& expr, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CountAllExpr, decltype(val_expr)>(expr, "=", val_expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator==(LiteralT&& literal, const CountAllExpr& expr) {
  return expr == std::forward<LiteralT>(literal);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(const CountAllExpr& expr, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CountAllExpr, decltype(val_expr)>(expr, "!=", val_expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator!=(LiteralT&& literal, const CountAllExpr& expr) {
  return expr != std::forward<LiteralT>(literal);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>(const CountAllExpr& expr, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CountAllExpr, decltype(val_expr)>(expr, ">", val_expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>(LiteralT&& literal, const CountAllExpr& expr) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<decltype(val_expr), CountAllExpr>(val_expr, ">", expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<(const CountAllExpr& expr, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CountAllExpr, decltype(val_expr)>(expr, "<", val_expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<(LiteralT&& literal, const CountAllExpr& expr) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<decltype(val_expr), CountAllExpr>(val_expr, "<", expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>=(const CountAllExpr& expr, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CountAllExpr, decltype(val_expr)>(expr, ">=", val_expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator>=(LiteralT&& literal, const CountAllExpr& expr) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<decltype(val_expr), CountAllExpr>(val_expr, ">=", expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<=(const CountAllExpr& expr, LiteralT&& literal) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<CountAllExpr, decltype(val_expr)>(expr, "<=", val_expr);
}

template <typename LiteralT>
  requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> ||
           std::is_convertible_v<std::remove_cvref_t<LiteralT>, std::string>
auto operator<=(LiteralT&& literal, const CountAllExpr& expr) {
  auto val_expr = val(std::forward<LiteralT>(literal));
  return BinaryCondition<decltype(val_expr), CountAllExpr>(val_expr, "<=", expr);
}

// Forward declaration for date expressions
class CurrentDateTimeExpr;
template <SqlExpr Expr>
class UnaryDateFunctionExpr;
template <SqlExpr Left, SqlExpr Right>
class BinaryDateFunctionExpr;
template <SqlExpr DateExpr, SqlExpr IntervalExpr>
class DateArithmeticExpr;

// Operators for CurrentDateTimeExpr with date columns
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>(const CurrentDateTimeExpr& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<CurrentDateTimeExpr, decltype(col_expr)>(expr, ">", col_expr);
}

template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<(const CurrentDateTimeExpr& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<CurrentDateTimeExpr, decltype(col_expr)>(expr, "<", col_expr);
}

template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>=(const CurrentDateTimeExpr& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<CurrentDateTimeExpr, decltype(col_expr)>(expr, ">=", col_expr);
}

template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<=(const CurrentDateTimeExpr& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<CurrentDateTimeExpr, decltype(col_expr)>(expr, "<=", col_expr);
}

template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator==(const CurrentDateTimeExpr& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<CurrentDateTimeExpr, decltype(col_expr)>(expr, "=", col_expr);
}

template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator!=(const CurrentDateTimeExpr& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<CurrentDateTimeExpr, decltype(col_expr)>(expr, "!=", col_expr);
}

// Operators for UnaryDateFunctionExpr with date columns
template <SqlExpr Expr, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>(const UnaryDateFunctionExpr<Expr>& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<UnaryDateFunctionExpr<Expr>, decltype(col_expr)>(expr, ">", col_expr);
}

template <SqlExpr Expr, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<(const UnaryDateFunctionExpr<Expr>& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<UnaryDateFunctionExpr<Expr>, decltype(col_expr)>(expr, "<", col_expr);
}

template <SqlExpr Expr, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>=(const UnaryDateFunctionExpr<Expr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<UnaryDateFunctionExpr<Expr>, decltype(col_expr)>(expr, ">=", col_expr);
}

template <SqlExpr Expr, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<=(const UnaryDateFunctionExpr<Expr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<UnaryDateFunctionExpr<Expr>, decltype(col_expr)>(expr, "<=", col_expr);
}

template <SqlExpr Expr, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator==(const UnaryDateFunctionExpr<Expr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<UnaryDateFunctionExpr<Expr>, decltype(col_expr)>(expr, "=", col_expr);
}

template <SqlExpr Expr, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator!=(const UnaryDateFunctionExpr<Expr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<UnaryDateFunctionExpr<Expr>, decltype(col_expr)>(expr, "!=", col_expr);
}

// Operators for BinaryDateFunctionExpr with date columns
template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>(const BinaryDateFunctionExpr<Left, Right>& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<BinaryDateFunctionExpr<Left, Right>, decltype(col_expr)>(expr, ">",
                                                                                  col_expr);
}

template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<(const BinaryDateFunctionExpr<Left, Right>& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<BinaryDateFunctionExpr<Left, Right>, decltype(col_expr)>(expr, "<",
                                                                                  col_expr);
}

template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>=(const BinaryDateFunctionExpr<Left, Right>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<BinaryDateFunctionExpr<Left, Right>, decltype(col_expr)>(expr,
                                                                                  ">=", col_expr);
}

template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<=(const BinaryDateFunctionExpr<Left, Right>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<BinaryDateFunctionExpr<Left, Right>, decltype(col_expr)>(expr,
                                                                                  "<=", col_expr);
}

template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator==(const BinaryDateFunctionExpr<Left, Right>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<BinaryDateFunctionExpr<Left, Right>, decltype(col_expr)>(expr, "=",
                                                                                  col_expr);
}

template <SqlExpr Left, SqlExpr Right, typename TableT, schema::FixedString Name, typename T,
          typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator!=(const BinaryDateFunctionExpr<Left, Right>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<BinaryDateFunctionExpr<Left, Right>, decltype(col_expr)>(expr,
                                                                                  "!=", col_expr);
}

// Operators for DateArithmeticExpr with date columns
template <SqlExpr DateExpr, SqlExpr IntervalExpr, typename TableT, schema::FixedString Name,
          typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>(const DateArithmeticExpr<DateExpr, IntervalExpr>& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<DateArithmeticExpr<DateExpr, IntervalExpr>, decltype(col_expr)>(expr, ">",
                                                                                         col_expr);
}

template <SqlExpr DateExpr, SqlExpr IntervalExpr, typename TableT, schema::FixedString Name,
          typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<(const DateArithmeticExpr<DateExpr, IntervalExpr>& expr,
               const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<DateArithmeticExpr<DateExpr, IntervalExpr>, decltype(col_expr)>(expr, "<",
                                                                                         col_expr);
}

template <SqlExpr DateExpr, SqlExpr IntervalExpr, typename TableT, schema::FixedString Name,
          typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator>=(const DateArithmeticExpr<DateExpr, IntervalExpr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<DateArithmeticExpr<DateExpr, IntervalExpr>, decltype(col_expr)>(
      expr, ">=", col_expr);
}

template <SqlExpr DateExpr, SqlExpr IntervalExpr, typename TableT, schema::FixedString Name,
          typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator<=(const DateArithmeticExpr<DateExpr, IntervalExpr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<DateArithmeticExpr<DateExpr, IntervalExpr>, decltype(col_expr)>(
      expr, "<=", col_expr);
}

template <SqlExpr DateExpr, SqlExpr IntervalExpr, typename TableT, schema::FixedString Name,
          typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator==(const DateArithmeticExpr<DateExpr, IntervalExpr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<DateArithmeticExpr<DateExpr, IntervalExpr>, decltype(col_expr)>(expr, "=",
                                                                                         col_expr);
}

template <SqlExpr DateExpr, SqlExpr IntervalExpr, typename TableT, schema::FixedString Name,
          typename T, typename... Modifiers>
  requires date_checking::DateTimeType<T>
auto operator!=(const DateArithmeticExpr<DateExpr, IntervalExpr>& expr,
                const schema::column<TableT, Name, T, Modifiers...>& col) {
  auto col_expr = to_expr(col);
  return BinaryCondition<DateArithmeticExpr<DateExpr, IntervalExpr>, decltype(col_expr)>(
      expr, "!=", col_expr);
}

}  // namespace query

}  // namespace relx

// Implementation of column methods that delegate to query functions
namespace relx::schema {

// Implementation for non-optional columns
template <typename TableT, FixedString Name, typename T, typename... Modifiers>
template <typename PatternType>
  requires std::convertible_to<PatternType, std::string>
auto column<TableT, Name, T, Modifiers...>::like(PatternType&& pattern) const {
  return query::like(*this, std::string(std::forward<PatternType>(pattern)));
}

template <typename TableT, FixedString Name, typename T, typename... Modifiers>
auto column<TableT, Name, T, Modifiers...>::is_null() const {
  return query::is_null(*this);
}

template <typename TableT, FixedString Name, typename T, typename... Modifiers>
auto column<TableT, Name, T, Modifiers...>::is_not_null() const {
  return query::is_not_null(*this);
}

// Implementation for optional columns
template <typename TableT, FixedString Name, typename T, typename... Modifiers>
template <typename PatternType>
  requires std::convertible_to<PatternType, std::string>
auto column<TableT, Name, std::optional<T>, Modifiers...>::like(PatternType&& pattern) const {
  return query::like(*this, std::string(std::forward<PatternType>(pattern)));
}

template <typename TableT, FixedString Name, typename T, typename... Modifiers>
auto column<TableT, Name, std::optional<T>, Modifiers...>::is_null() const {
  return query::is_null(*this);
}

template <typename TableT, FixedString Name, typename T, typename... Modifiers>
auto column<TableT, Name, std::optional<T>, Modifiers...>::is_not_null() const {
  return query::is_not_null(*this);
}

}  // namespace relx::schema