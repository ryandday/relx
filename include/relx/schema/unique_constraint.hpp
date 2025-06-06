#pragma once

#include "core.hpp"
#include "meta.hpp"

#include <string_view>
#include <type_traits>

namespace relx::schema {

/// @brief Represents a UNIQUE constraint on a table
/// @tparam ColumnPtr Pointer to the column member
template <auto ColumnPtr>
class unique_constraint {
public:
  /// @brief Get SQL definition for the UNIQUE constraint
  /// @return SQL string defining the constraint
  std::string sql_definition() const {
    // Extract the column name from the pointer
    using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;

    return "UNIQUE (" + std::string(column_type::name) + ")";
  }
};

/// @brief Represents a composite UNIQUE constraint on multiple columns
/// @tparam ColumnPtrs Pointers to the columns that form the unique constraint
template <auto... ColumnPtrs>
class composite_unique_constraint {
public:
  /// @brief Default constructor
  composite_unique_constraint() = default;

  /// @brief Get SQL definition for the composite UNIQUE constraint
  /// @return SQL string defining the constraint
  std::string sql_definition() const {
    std::string result = "UNIQUE (";
    result += get_column_names();
    result += ")";
    return result;
  }

private:
  // Helper to get comma-separated column names
  std::string get_column_names() const {
    // Using fold expression to concatenate column names
    std::string names;
    (append_column_name<ColumnPtrs>(names, names.empty() ? "" : ", "), ...);
    return names;
  }

  // Helper to append a single column name
  template <auto ColumnPtr>
  void append_column_name(std::string& names, const std::string& separator) const {
    using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;
    names += separator + std::string(column_type::name);
  }
};

}  // namespace relx::schema