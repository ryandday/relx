#pragma once

#include "column.hpp"
#include "table.hpp"

#include <string_view>
#include <type_traits>
#include <vector>

namespace relx::schema {

/// @brief Index types
enum class index_type { normal, unique, fulltext, spatial };

/// @brief Convert index type to SQL string
/// @param type The index type
/// @return SQL string for the index type
constexpr std::string_view index_type_to_string(index_type type) {
  switch (type) {
  case index_type::unique:
    return "UNIQUE ";
  case index_type::fulltext:
    return "FULLTEXT ";
  case index_type::spatial:
    return "SPATIAL ";
  case index_type::normal:  // Fall through
  default:
    return "";
  }
}

/// @brief Represents an index on a table
/// @tparam ColumnPtr Pointer to the column member
template <auto ColumnPtr>
class index {
public:
  /// @brief Default constructor - creates a normal index
  index() : type_(index_type::normal) {}

  /// @brief Constructor with specific index type
  /// @param type The type of index to create
  explicit index(index_type type) : type_(type) {}

  /// @brief Get SQL statement for creating the index
  /// @return SQL CREATE INDEX statement
  std::string create_index_sql() const {
    // Extract the column name from the pointer
    using table_type = typename member_pointer_class<decltype(ColumnPtr)>::type;
    using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;

    // Get the table name directly
    std::string table_name = std::string(table_type::table_name);
    std::string column_name = std::string(column_type::name);

    std::string index_name = table_name + "_" + column_name + "_idx";

    std::string result = "CREATE ";
    result += std::string(index_type_to_string(type_));
    result += "INDEX " + index_name + " ON ";
    result += table_name + " (" + column_name + ")";

    return result;
  }

private:
  index_type type_;

  // Helper to extract the class type from a member pointer
  template <typename T>
  struct member_pointer_class;

  template <typename C, typename T>
  struct member_pointer_class<T C::*> {
    using type = C;
  };

  // Helper to extract the member type from a member pointer
  template <typename T>
  struct member_pointer_type;

  template <typename C, typename T>
  struct member_pointer_type<T C::*> {
    using type = T;
  };
};

/// @brief Represents a composite index on multiple columns
/// @tparam ColumnPtrs Pointers to the columns that form the index
template <auto... ColumnPtrs>
class composite_index {
public:
  /// @brief Default constructor - creates a normal index
  composite_index() : type_(index_type::normal) {}

  /// @brief Constructor with specific index type
  /// @param type The type of index to create
  explicit composite_index(index_type type) : type_(type) {}

  /// @brief Get SQL statement for creating the composite index
  /// @return SQL CREATE INDEX statement
  std::string create_index_sql() const {
    // Get the table name (assuming all columns are from the same table)
    std::string table_name = get_table_name<0>();

    // Build the column list and collect all column names
    std::string column_list;
    std::vector<std::string> column_names;

    // Process all column pointers to build column list and names
    (append_column_info<ColumnPtrs>(column_list, column_names, column_list.empty() ? "" : ", "),
     ...);

    // Generate index name from table name and column names
    std::string index_name = table_name + "_";
    for (size_t i = 0; i < column_names.size(); ++i) {
      index_name += column_names[i];
      if (i < column_names.size() - 1) {
        index_name += "_";
      }
    }
    index_name += "_idx";

    // Build the CREATE INDEX statement
    std::string result = "CREATE ";
    result += std::string(index_type_to_string(type_));
    result += "INDEX " + index_name + " ON ";
    result += table_name + " (" + column_list + ")";

    return result;
  }

private:
  index_type type_;

  // Helper to get the table name using the first column in the parameter pack
  template <size_t I>
  std::string get_table_name() const {
    if constexpr (I < sizeof...(ColumnPtrs)) {
      using first_col_ptr_t = std::tuple_element_t<I, std::tuple<decltype(ColumnPtrs)...>>;
      using table_t =
          typename std::remove_reference_t<typename table_for_column<first_col_ptr_t>::type>;
      return std::string(table_t::table_name);
    }
    return "unknown_table";
  }

  // Helper type to extract the table type from a column pointer
  template <typename T>
  struct table_for_column;

  template <typename C, typename T>
  struct table_for_column<T C::*> {
    using type = C;
  };

  // Helper to append column info to the column list and index name
  template <auto ColumnPtr>
  void append_column_info(std::string& column_list, std::vector<std::string>& column_names,
                          const std::string& separator) const {
    using col_type = decltype(ColumnPtr);
    using column_type = typename std::remove_reference_t<typename column_type_for<col_type>::type>;
    std::string column_name = std::string(column_type::name);

    // Add to column list with separator if needed
    column_list += separator + column_name;

    // Add to column names vector
    column_names.push_back(column_name);
  }

  // Helper to extract the column type from a column pointer
  template <typename T>
  struct column_type_for;

  template <typename C, typename T>
  struct column_type_for<T C::*> {
    using type = T;
  };
};

}  // namespace relx::schema
