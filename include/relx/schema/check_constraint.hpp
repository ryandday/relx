#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include "meta.hpp"

#include <string>
#include <string_view>

namespace relx::schema {

/// @brief Check constraint that accepts a condition string at compile time
/// @details Uses compile-time string literals to define the constraint condition
/// @tparam Condition The SQL condition as a FixedString
/// @tparam Name Optional name for the constraint
template <FixedString Condition, FixedString Name = "">
class table_check_constraint {
public:
  /// @brief Get SQL definition for the CHECK constraint
  /// @return SQL string defining the constraint
  constexpr std::string sql_definition() const {
    std::string result;

    // Add constraint name if provided
    if constexpr (!Name.empty()) {
      result = "CONSTRAINT " + std::string(std::string_view(Name)) + " ";
    }

    // Use the condition as-is
    result += "CHECK (" + std::string(std::string_view(Condition)) + ")";
    return result;
  }

  /// @brief Get the condition as a string_view
  /// @return The condition string
  constexpr std::string_view condition() const { return std::string_view(Condition); }

  /// @brief Get the name as a string_view
  /// @return The constraint name
  constexpr std::string_view name() const { return std::string_view(Name); }
};

/// @brief Check constraint explicitly associated with a column
/// @details Uses compile-time string literals for the condition and binds to a specific column
/// @tparam ColumnPtr Pointer to the column to which the constraint applies
/// @tparam Condition The SQL condition as a FixedString
/// @tparam Name Optional name for the constraint
template <auto ColumnPtr, FixedString Condition, FixedString Name = "">
class column_check_constraint {
public:
  /// @brief Get SQL definition for the column-specific CHECK constraint
  /// @return SQL string defining the constraint
  constexpr std::string sql_definition() const {
    std::string result;

    // Add constraint name if provided
    if constexpr (!Name.empty()) {
      result = "CONSTRAINT " + std::string(std::string_view(Name)) + " ";
    }

    // Use the column name and condition
    using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;

    // Append the condition, replacing "column" or "COLUMN" with the actual column name if needed
    std::string condition_str = std::string(std::string_view(Condition));
    result += "CHECK (" + condition_str + ")";

    return result;
  }

  /// @brief Get the condition as a string_view
  /// @return The condition string
  constexpr std::string_view condition() const { return std::string_view(Condition); }

  /// @brief Get the name as a string_view
  /// @return The constraint name
  constexpr std::string_view name() const { return std::string_view(Name); }

  /// @brief Get the associated column name
  /// @return The column name
  std::string_view column_name() const {
    using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;
    return column_type::name;
  }
};

/// @brief Helper function to create a named check constraint at compile time
/// @tparam Condition The SQL condition
/// @tparam Name The constraint name
/// @return A check constraint with the given condition and name
template <FixedString Condition, FixedString Name>
constexpr auto named_check() {
  return table_check_constraint<Condition, Name>();
}

/// @brief Helper function to create an unnamed check constraint at compile time
/// @tparam Condition The SQL condition
/// @return A check constraint with the given condition
template <FixedString Condition>
constexpr auto table_check() {
  return table_check_constraint<Condition, "">();
}

/// @brief Helper function to create a column-bound check constraint at compile time
/// @tparam ColumnPtr Pointer to the column
/// @tparam Condition The SQL condition
/// @return A column-specific check constraint
template <auto ColumnPtr, FixedString Condition>
constexpr auto column_check() {
  return column_check_constraint<ColumnPtr, Condition, "">();
}

/// @brief Helper function to create a named column-bound check constraint at compile time
/// @tparam ColumnPtr Pointer to the column
/// @tparam Condition The SQL condition
/// @tparam Name The constraint name
/// @return A named column-specific check constraint
template <auto ColumnPtr, FixedString Condition, FixedString Name>
constexpr auto named_column_check() {
  return column_check_constraint<ColumnPtr, Condition, Name>();
}

}  // namespace relx::schema
