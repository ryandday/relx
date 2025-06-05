#pragma once

#include "core.hpp"
#include "meta.hpp"

#include <string_view>
#include <type_traits>

namespace relx::schema {

/// @brief Represents a primary key constraint on a table
/// @tparam ColumnPtr Pointer to the column member
template <auto ColumnPtr>
class table_primary_key {
public:
  /// @brief Default constructor
  table_primary_key() = default;

  /// @brief Get SQL definition for the PRIMARY KEY constraint
  /// @return SQL string defining the constraint
  std::string sql_definition() const {
    // Extract the column name from the pointer
    using table_type = typename member_pointer_class<decltype(ColumnPtr)>::type;
    using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;

    return "PRIMARY KEY (" + std::string(column_type::name) + ")";
  }

private:
};

/// @brief Represents a composite primary key constraint on multiple columns
/// @tparam ColumnPtrs Pointers to the columns that form the primary key
template <auto... ColumnPtrs>
class composite_primary_key {
public:
  /// @brief Default constructor
  composite_primary_key() = default;

  /// @brief Get SQL definition for the composite PRIMARY KEY constraint
  /// @return SQL string defining the constraint
  std::string sql_definition() const {
    std::string result = "PRIMARY KEY (";
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

/// @brief Helper function to create a primary key
/// @tparam ColumnPtrs Pointers to the columns that form the primary key
/// @return A primary key constraint
template <auto... ColumnPtrs>
auto make_pk() {
  if constexpr (sizeof...(ColumnPtrs) == 1) {
    // Use fold expression to extract the single column pointer
    return table_primary_key<(
        []<auto Ptr>() {
          return Ptr;
        }.template operator()<ColumnPtrs>(),
        ...)>();
  } else {
    return composite_primary_key<ColumnPtrs...>();
  }
}

/// @brief Helper type alias for primary key constraints
/// @details Automatically selects between single-column and composite primary key based on the
/// number of columns
template <auto... ColumnPtrs>
using pk = decltype(make_pk<ColumnPtrs...>());

}  // namespace relx::schema
