#pragma once

#include "../schema/column.hpp"
#include "../query/schema_adapter.hpp"
#include "../query/value.hpp"
#include "../query/condition.hpp"
#include "../query/meta.hpp"
#include "../query/function.hpp"

#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace sqllib {
namespace schema {

// Binary comparison operators for columns

// Equality comparison with direct values
template <FixedString Name, typename T, typename DefaultValueType, typename ValueType>
auto operator==(const column<Name, T, DefaultValueType>& col, const ValueType& value) {
    auto col_expr = query::to_expr(col);
    auto val_expr = query::val(value);
    return col_expr == val_expr;
}

// Inequality comparison with direct values
template <FixedString Name, typename T, typename DefaultValueType, typename ValueType>
auto operator!=(const column<Name, T, DefaultValueType>& col, const ValueType& value) {
    auto col_expr = query::to_expr(col);
    auto val_expr = query::val(value);
    return col_expr != val_expr;
}

// Greater than comparison with direct values
template <FixedString Name, typename T, typename DefaultValueType, typename ValueType>
auto operator>(const column<Name, T, DefaultValueType>& col, const ValueType& value) {
    auto col_expr = query::to_expr(col);
    auto val_expr = query::val(value);
    return col_expr > val_expr;
}

// Less than comparison with direct values
template <FixedString Name, typename T, typename DefaultValueType, typename ValueType>
auto operator<(const column<Name, T, DefaultValueType>& col, const ValueType& value) {
    auto col_expr = query::to_expr(col);
    auto val_expr = query::val(value);
    return col_expr < val_expr;
}

// Greater than or equal comparison with direct values
template <FixedString Name, typename T, typename DefaultValueType, typename ValueType>
auto operator>=(const column<Name, T, DefaultValueType>& col, const ValueType& value) {
    auto col_expr = query::to_expr(col);
    auto val_expr = query::val(value);
    return col_expr >= val_expr;
}

// Less than or equal comparison with direct values
template <FixedString Name, typename T, typename DefaultValueType, typename ValueType>
auto operator<=(const column<Name, T, DefaultValueType>& col, const ValueType& value) {
    auto col_expr = query::to_expr(col);
    auto val_expr = query::val(value);
    return col_expr <= val_expr;
}

// Column to column equality comparison
template <FixedString Name1, typename T1, typename DefaultValueType1,
          FixedString Name2, typename T2, typename DefaultValueType2>
auto operator==(const column<Name1, T1, DefaultValueType1>& col1, 
                const column<Name2, T2, DefaultValueType2>& col2) {
    auto col1_expr = query::to_expr(col1);
    auto col2_expr = query::to_expr(col2);
    return col1_expr == col2_expr;
}

// Column to column inequality comparison
template <FixedString Name1, typename T1, typename DefaultValueType1,
          FixedString Name2, typename T2, typename DefaultValueType2>
auto operator!=(const column<Name1, T1, DefaultValueType1>& col1, 
                const column<Name2, T2, DefaultValueType2>& col2) {
    auto col1_expr = query::to_expr(col1);
    auto col2_expr = query::to_expr(col2);
    return col1_expr != col2_expr;
}

// Column to column greater than comparison
template <FixedString Name1, typename T1, typename DefaultValueType1,
          FixedString Name2, typename T2, typename DefaultValueType2>
auto operator>(const column<Name1, T1, DefaultValueType1>& col1, 
               const column<Name2, T2, DefaultValueType2>& col2) {
    auto col1_expr = query::to_expr(col1);
    auto col2_expr = query::to_expr(col2);
    return col1_expr > col2_expr;
}

// Column to column less than comparison
template <FixedString Name1, typename T1, typename DefaultValueType1,
          FixedString Name2, typename T2, typename DefaultValueType2>
auto operator<(const column<Name1, T1, DefaultValueType1>& col1, 
               const column<Name2, T2, DefaultValueType2>& col2) {
    auto col1_expr = query::to_expr(col1);
    auto col2_expr = query::to_expr(col2);
    return col1_expr < col2_expr;
}

// Column to column greater than or equal comparison
template <FixedString Name1, typename T1, typename DefaultValueType1,
          FixedString Name2, typename T2, typename DefaultValueType2>
auto operator>=(const column<Name1, T1, DefaultValueType1>& col1, 
                const column<Name2, T2, DefaultValueType2>& col2) {
    auto col1_expr = query::to_expr(col1);
    auto col2_expr = query::to_expr(col2);
    return col1_expr >= col2_expr;
}

// Column to column less than or equal comparison
template <FixedString Name1, typename T1, typename DefaultValueType1,
          FixedString Name2, typename T2, typename DefaultValueType2>
auto operator<=(const column<Name1, T1, DefaultValueType1>& col1, 
                const column<Name2, T2, DefaultValueType2>& col2) {
    auto col1_expr = query::to_expr(col1);
    auto col2_expr = query::to_expr(col2);
    return col1_expr <= col2_expr;
}

// Specialized operators for common types

// Boolean negation operator
template <FixedString Name, typename DefaultValueType>
auto operator!(const column<Name, bool, DefaultValueType>& col) {
    auto col_expr = query::to_expr(col);
    // Using the logical NOT operator
    return !col_expr;
}

// Logical AND operator between a column and a query::SqlExpr
template <FixedString Name, typename DefaultValueType, query::SqlExpr Expr>
auto operator&&(const column<Name, bool, DefaultValueType>& col, const Expr& expr) {
    auto col_expr = query::to_expr(col);
    return col_expr && expr;
}

// Logical AND operator between a query::SqlExpr and a column (reverse)
template <query::SqlExpr Expr, FixedString Name, typename DefaultValueType>
auto operator&&(const Expr& expr, const column<Name, bool, DefaultValueType>& col) {
    auto col_expr = query::to_expr(col);
    return expr && col_expr;
}

// Logical OR operator between a column and a query::SqlExpr
template <FixedString Name, typename DefaultValueType, query::SqlExpr Expr>
auto operator||(const column<Name, bool, DefaultValueType>& col, const Expr& expr) {
    auto col_expr = query::to_expr(col);
    return col_expr || expr;
}

// Logical OR operator between a query::SqlExpr and a column (reverse)
template <query::SqlExpr Expr, FixedString Name, typename DefaultValueType>
auto operator||(const Expr& expr, const column<Name, bool, DefaultValueType>& col) {
    auto col_expr = query::to_expr(col);
    return expr || col_expr;
}

// Symmetrical operators (value on left, column on right)

// Equality comparison with direct values (reversed)
template <typename ValueType, FixedString Name, typename T, typename DefaultValueType>
auto operator==(const ValueType& value, const column<Name, T, DefaultValueType>& col) {
    return col == value;
}

// Inequality comparison with direct values (reversed)
template <typename ValueType, FixedString Name, typename T, typename DefaultValueType>
auto operator!=(const ValueType& value, const column<Name, T, DefaultValueType>& col) {
    return col != value;
}

// Greater than comparison with direct values (reversed)
template <typename ValueType, FixedString Name, typename T, typename DefaultValueType>
auto operator>(const ValueType& value, const column<Name, T, DefaultValueType>& col) {
    return col < value;
}

// Less than comparison with direct values (reversed)
template <typename ValueType, FixedString Name, typename T, typename DefaultValueType>
auto operator<(const ValueType& value, const column<Name, T, DefaultValueType>& col) {
    return col > value;
}

// Greater than or equal comparison with direct values (reversed)
template <typename ValueType, FixedString Name, typename T, typename DefaultValueType>
auto operator>=(const ValueType& value, const column<Name, T, DefaultValueType>& col) {
    return col <= value;
}

// Less than or equal comparison with direct values (reversed)
template <typename ValueType, FixedString Name, typename T, typename DefaultValueType>
auto operator<=(const ValueType& value, const column<Name, T, DefaultValueType>& col) {
    return col >= value;
}

// Boolean operators for columns with conditions

// Logical AND between column and any condition
template <FixedString Name, typename DefaultValueType, typename Cond>
requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator&&(const column<Name, bool, DefaultValueType>& col, const Cond& cond) {
    auto col_expr = query::to_expr(col);
    if constexpr (query::SqlExpr<Cond>) {
        return col_expr && cond;
    } else {
        auto val_expr = query::val(cond);
        return col_expr && val_expr;
    }
}

// Logical OR between column and any condition
template <FixedString Name, typename DefaultValueType, typename Cond>
requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator||(const column<Name, bool, DefaultValueType>& col, const Cond& cond) {
    auto col_expr = query::to_expr(col);
    if constexpr (query::SqlExpr<Cond>) {
        return col_expr || cond;
    } else {
        auto val_expr = query::val(cond);
        return col_expr || val_expr;
    }
}

// Logical AND between condition and column (reversed)
template <typename Cond, FixedString Name, typename DefaultValueType>
requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator&&(const Cond& cond, const column<Name, bool, DefaultValueType>& col) {
    auto col_expr = query::to_expr(col);
    if constexpr (query::SqlExpr<Cond>) {
        return cond && col_expr;
    } else {
        auto val_expr = query::val(cond);
        return val_expr && col_expr;
    }
}

// Logical OR between condition and column (reversed)
template <typename Cond, FixedString Name, typename DefaultValueType>
requires query::SqlExpr<Cond> || (!std::same_as<Cond, bool>)
auto operator||(const Cond& cond, const column<Name, bool, DefaultValueType>& col) {
    auto col_expr = query::to_expr(col);
    if constexpr (query::SqlExpr<Cond>) {
        return cond || col_expr;
    } else {
        auto val_expr = query::val(cond);
        return val_expr || col_expr;
    }
}

} // namespace schema

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
}

// Equality comparison with Value
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ValueT>
auto operator==(const schema::column<Name, T, DefaultValueType>& col, const Value<ValueT>& value) {
    auto col_expr = to_expr(col);
    return col_expr == value;
}

// Inequality comparison with Value
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ValueT>
auto operator!=(const schema::column<Name, T, DefaultValueType>& col, const Value<ValueT>& value) {
    auto col_expr = to_expr(col);
    return col_expr != value;
}

// Greater than comparison with Value
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ValueT>
auto operator>(const schema::column<Name, T, DefaultValueType>& col, const Value<ValueT>& value) {
    auto col_expr = to_expr(col);
    return col_expr > value;
}

// Less than comparison with Value
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ValueT>
auto operator<(const schema::column<Name, T, DefaultValueType>& col, const Value<ValueT>& value) {
    auto col_expr = to_expr(col);
    return col_expr < value;
}

// Greater than or equal comparison with Value
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ValueT>
auto operator>=(const schema::column<Name, T, DefaultValueType>& col, const Value<ValueT>& value) {
    auto col_expr = to_expr(col);
    return col_expr >= value;
}

// Less than or equal comparison with Value
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ValueT>
auto operator<=(const schema::column<Name, T, DefaultValueType>& col, const Value<ValueT>& value) {
    auto col_expr = to_expr(col);
    return col_expr <= value;
}

// Column comparison with direct numeric literals
// These overloads allow for expressions like col > 42 without needing query::val(42)

// Equality comparison with numeric literal
template <schema::FixedString Name, typename T, typename DefaultValueType, typename LiteralT>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator==(const schema::column<Name, T, DefaultValueType>& col, LiteralT&& literal) {
    auto col_expr = to_expr(col);
    auto val_expr = val(std::forward<LiteralT>(literal));
    return col_expr == val_expr;
}

// Inequality comparison with numeric literal
template <schema::FixedString Name, typename T, typename DefaultValueType, typename LiteralT>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator!=(const schema::column<Name, T, DefaultValueType>& col, LiteralT&& literal) {
    auto col_expr = to_expr(col);
    auto val_expr = val(std::forward<LiteralT>(literal));
    return col_expr != val_expr;
}

// Greater than comparison with numeric literal
template <schema::FixedString Name, typename T, typename DefaultValueType, typename LiteralT>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>(const schema::column<Name, T, DefaultValueType>& col, LiteralT&& literal) {
    auto col_expr = to_expr(col);
    auto val_expr = val(std::forward<LiteralT>(literal));
    return col_expr > val_expr;
}

// Less than comparison with numeric literal
template <schema::FixedString Name, typename T, typename DefaultValueType, typename LiteralT>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<(const schema::column<Name, T, DefaultValueType>& col, LiteralT&& literal) {
    auto col_expr = to_expr(col);
    auto val_expr = val(std::forward<LiteralT>(literal));
    return col_expr < val_expr;
}

// Greater than or equal comparison with numeric literal
template <schema::FixedString Name, typename T, typename DefaultValueType, typename LiteralT>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>=(const schema::column<Name, T, DefaultValueType>& col, LiteralT&& literal) {
    auto col_expr = to_expr(col);
    auto val_expr = val(std::forward<LiteralT>(literal));
    return col_expr >= val_expr;
}

// Less than or equal comparison with numeric literal
template <schema::FixedString Name, typename T, typename DefaultValueType, typename LiteralT>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<=(const schema::column<Name, T, DefaultValueType>& col, LiteralT&& literal) {
    auto col_expr = to_expr(col);
    auto val_expr = val(std::forward<LiteralT>(literal));
    return col_expr <= val_expr;
}

// Symmetrical operators for numeric literals (literal on left, column on right)

// Equality comparison with numeric literal (reversed)
template <typename LiteralT, schema::FixedString Name, typename T, typename DefaultValueType>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator==(LiteralT&& literal, const schema::column<Name, T, DefaultValueType>& col) {
    return col == std::forward<LiteralT>(literal);
}

// Inequality comparison with numeric literal (reversed)
template <typename LiteralT, schema::FixedString Name, typename T, typename DefaultValueType>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator!=(LiteralT&& literal, const schema::column<Name, T, DefaultValueType>& col) {
    return col != std::forward<LiteralT>(literal);
}

// Greater than comparison with numeric literal (reversed)
template <typename LiteralT, schema::FixedString Name, typename T, typename DefaultValueType>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>(LiteralT&& literal, const schema::column<Name, T, DefaultValueType>& col) {
    return col < std::forward<LiteralT>(literal);
}

// Less than comparison with numeric literal (reversed)
template <typename LiteralT, schema::FixedString Name, typename T, typename DefaultValueType>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<(LiteralT&& literal, const schema::column<Name, T, DefaultValueType>& col) {
    return col > std::forward<LiteralT>(literal);
}

// Greater than or equal comparison with numeric literal (reversed)
template <typename LiteralT, schema::FixedString Name, typename T, typename DefaultValueType>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator>=(LiteralT&& literal, const schema::column<Name, T, DefaultValueType>& col) {
    return col <= std::forward<LiteralT>(literal);
}

// Less than or equal comparison with numeric literal (reversed)
template <typename LiteralT, schema::FixedString Name, typename T, typename DefaultValueType>
requires std::is_arithmetic_v<std::remove_cvref_t<LiteralT>> && 
         (!std::is_same_v<LiteralT, bool>) && 
         (!meta::is_specialization_v<std::remove_cvref_t<LiteralT>, Value>)
auto operator<=(LiteralT&& literal, const schema::column<Name, T, DefaultValueType>& col) {
    return col >= std::forward<LiteralT>(literal);
}

// String literal comparison
// These allow for using string literals directly without query::val

// Equality comparison with string literal
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto operator==(const schema::column<Name, T, DefaultValueType>& col, const char* str) {
    auto col_expr = to_expr(col);
    auto val_expr = val(str);
    return col_expr == val_expr;
}

// Inequality comparison with string literal
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto operator!=(const schema::column<Name, T, DefaultValueType>& col, const char* str) {
    auto col_expr = to_expr(col);
    auto val_expr = val(str);
    return col_expr != val_expr;
}

// String literal comparison (reversed)
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto operator==(const char* str, const schema::column<Name, T, DefaultValueType>& col) {
    return col == str;
}

template <schema::FixedString Name, typename T, typename DefaultValueType>
auto operator!=(const char* str, const schema::column<Name, T, DefaultValueType>& col) {
    return col != str;
}

// Additional column operation overloads to avoid to_expr wrappers

// LIKE operator for columns
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto like(const schema::column<Name, T, DefaultValueType>& col, std::string pattern) {
    auto col_expr = to_expr(col);
    return like(col_expr, std::move(pattern));
}

// IN operator for columns
template <schema::FixedString Name, typename T, typename DefaultValueType, 
          std::ranges::range Range>
requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
auto in(const schema::column<Name, T, DefaultValueType>& col, Range values) {
    auto col_expr = to_expr(col);
    return in(col_expr, std::move(values));
}

// IS NULL operator for columns
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto is_null(const schema::column<Name, T, DefaultValueType>& col) {
    auto col_expr = to_expr(col);
    return is_null(col_expr);
}

// IS NOT NULL operator for columns
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto is_not_null(const schema::column<Name, T, DefaultValueType>& col) {
    auto col_expr = to_expr(col);
    return is_not_null(col_expr);
}

// BETWEEN operator for columns
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto between(const schema::column<Name, T, DefaultValueType>& col, 
             std::string lower, std::string upper) {
    auto col_expr = to_expr(col);
    return between(col_expr, std::move(lower), std::move(upper));
}

// Column support for case expressions

// When with a column condition
template <schema::FixedString Name, typename T, typename DefaultValueType, typename ResultT>
auto when(const schema::column<Name, T, DefaultValueType>& condition, 
          const query::Value<ResultT>& result) {
    auto col_expr = to_expr(condition);
    return when(col_expr, result);
}

// When with a column result
template <typename CondT, schema::FixedString Name, typename T, typename DefaultValueType>
auto when(const CondT& condition, 
          const schema::column<Name, T, DefaultValueType>& result) {
    auto result_expr = to_expr(result);
    return when(condition, result_expr);
}

// Else with a column result
template <schema::FixedString Name, typename T, typename DefaultValueType>
auto else_(const schema::column<Name, T, DefaultValueType>& result) {
    auto result_expr = to_expr(result);
    return else_(result_expr);
}

// Column support for select expressions
template <schema::FixedString Name, typename T, typename DefaultValueType, typename... Args>
auto select_expr(const schema::column<Name, T, DefaultValueType>& col, Args&&... args) {
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
    return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(val_expr)>(coalesce, "=", val_expr);
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
    return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(str_val)>(coalesce, "=", str_val);
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
    return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(val_expr)>(coalesce, "!=", val_expr);
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
    return BinaryCondition<CoalesceExpr<First, Second, Rest...>, decltype(str_val)>(coalesce, "!=", str_val);
}

template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto operator!=(const char* str, const CoalesceExpr<First, Second, Rest...>& coalesce) {
    return coalesce != str;
}

} // namespace query
} // namespace sqllib