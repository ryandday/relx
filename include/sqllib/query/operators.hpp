#pragma once

#include "../schema/column.hpp"
#include "../query/schema_adapter.hpp"
#include "../query/value.hpp"
#include "../query/condition.hpp"
#include "../query/meta.hpp"

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

} // namespace schema

namespace query {

// Column to SQL Value comparisons (for _sql literals)

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

// Symmetrical operators for Value (Value on left, column on right)

// Equality comparison with Value (reversed)
template <typename ValueT, schema::FixedString Name, typename T, typename DefaultValueType>
auto operator==(const Value<ValueT>& value, const schema::column<Name, T, DefaultValueType>& col) {
    return col == value;
}

// Inequality comparison with Value (reversed)
template <typename ValueT, schema::FixedString Name, typename T, typename DefaultValueType>
auto operator!=(const Value<ValueT>& value, const schema::column<Name, T, DefaultValueType>& col) {
    return col != value;
}

// Greater than comparison with Value (reversed)
template <typename ValueT, schema::FixedString Name, typename T, typename DefaultValueType>
auto operator>(const Value<ValueT>& value, const schema::column<Name, T, DefaultValueType>& col) {
    return col < value;
}

// Less than comparison with Value (reversed)
template <typename ValueT, schema::FixedString Name, typename T, typename DefaultValueType>
auto operator<(const Value<ValueT>& value, const schema::column<Name, T, DefaultValueType>& col) {
    return col > value;
}

// Greater than or equal comparison with Value (reversed)
template <typename ValueT, schema::FixedString Name, typename T, typename DefaultValueType>
auto operator>=(const Value<ValueT>& value, const schema::column<Name, T, DefaultValueType>& col) {
    return col <= value;
}

// Less than or equal comparison with Value (reversed)
template <typename ValueT, schema::FixedString Name, typename T, typename DefaultValueType>
auto operator<=(const Value<ValueT>& value, const schema::column<Name, T, DefaultValueType>& col) {
    return col >= value;
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

} // namespace query
} // namespace sqllib