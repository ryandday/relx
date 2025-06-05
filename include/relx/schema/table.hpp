#pragma once

#include "column.hpp"
#include "fixed_string.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/pfr.hpp>

namespace relx::schema {

/// @brief Helper to detect column members in a table
template <typename T>
concept is_column = requires {
  { T::name } -> std::convertible_to<std::string_view>;
  T::sql_type;
  std::declval<T>().sql_definition();
};

/// @brief Concept for a database table type
/// @details Requires the type to have a static constexpr table_name member that is
/// convertible to std::string_view.
/// Example: static constexpr auto table_name = "users";
template <typename T>
concept TableConcept = requires {
  { T::table_name } -> std::convertible_to<std::string_view>;
  requires std::is_const_v<std::remove_reference_t<decltype(T::table_name)>>;  // Ensure it's
                                                                               // const/constexpr
};

/// @brief Helper to detect constraint members in a table
template <typename T>
concept is_constraint = requires(T t) {
  { t.sql_definition() } -> std::convertible_to<std::string>;
} && !is_column<T>;  // Ensure a constraint is not also a column

/// @brief Generate SQL column definitions from a table struct using Boost PFR
/// @tparam Table The table struct type
/// @param table_instance An instance of the table
/// @return String containing SQL column definitions
template <TableConcept Table>
std::string collect_column_definitions(const Table& table_instance) {
  std::vector<std::string> column_defs;
  std::unordered_set<std::string> added_columns;  // Track added columns by name

  boost::pfr::for_each_field(table_instance, [&](const auto& field) {
    if constexpr (is_column<std::remove_cvref_t<decltype(field)>>) {
      // Use column name as key to avoid duplicates
      std::string col_name = std::string(std::remove_cvref_t<decltype(field)>::name);
      if (added_columns.find(col_name) == added_columns.end()) {
        column_defs.push_back(field.sql_definition());
        added_columns.insert(col_name);
      }
    }
  });

  // Join the column definitions with commas
  if (column_defs.empty()) {
    return "";
  }

  std::string result = column_defs[0];
  for (size_t i = 1; i < column_defs.size(); ++i) {
    result += ",\n" + column_defs[i];
  }

  return result;
}

/// @brief Generate SQL constraint definitions from a table struct
/// @tparam Table The table struct type
/// @param table_instance An instance of the table
/// @return String containing SQL constraint definitions
template <TableConcept Table>
std::string collect_constraint_definitions(const Table& table_instance) {
  std::vector<std::string> constraint_defs;
  std::unordered_set<std::string> added_constraints;  // Track constraints to avoid duplicates

  boost::pfr::for_each_field(table_instance, [&](const auto& field) {
    if constexpr (is_constraint<std::remove_cvref_t<decltype(field)>>) {
      std::string constraint = field.sql_definition();
      if (added_constraints.find(constraint) == added_constraints.end()) {
        constraint_defs.push_back(constraint);
        added_constraints.insert(constraint);
      }
    }
  });

  // Join the constraint definitions with commas
  if (constraint_defs.empty()) {
    return "";
  }

  std::string result = constraint_defs[0];
  for (size_t i = 1; i < constraint_defs.size(); ++i) {
    result += ",\n" + constraint_defs[i];
  }

  return result;
}

/// @brief Generate CREATE TABLE SQL statement for a table struct
/// @tparam Table The table struct type
/// @param table_instance An instance of the table
/// @return SQL string to create the table
template <TableConcept Table>
class create_table {
private:
  const Table& table_instance_;
  bool if_not_exists_ = false;
  bool if_exists_ = false;

  bool cascade_ = false;
  bool restrict_ = false;

  std::vector<std::string> bind_params_;

public:
  create_table(const Table& table_instance) : table_instance_(table_instance) {}

  create_table& if_not_exists(bool if_not_exists = true) {
    if_not_exists_ = if_not_exists;
    return *this;
  }

  create_table& if_exists(bool if_exists = true) {
    if_exists_ = if_exists;
    return *this;
  }

  create_table& cascade(bool cascade = true) {
    cascade_ = cascade;
    return *this;
  }

  create_table& restrict(bool restrict = true) {
    restrict_ = restrict;
    return *this;
  }

  std::string to_sql() const {
    std::string sql = "CREATE TABLE ";
    if (if_not_exists_) {
      sql += "IF NOT EXISTS ";
    }
    if (if_exists_) {
      sql += "IF EXISTS ";
    }

    if (if_exists_ && if_not_exists_) {
      throw std::invalid_argument("if_exists and if_not_exists cannot both be true");
    }

    sql += std::string(Table::table_name) + " (\n";

    // Add column definitions
    sql += collect_column_definitions(table_instance_);

    // Add constraint definitions
    std::string constraints = collect_constraint_definitions(table_instance_);
    if (!constraints.empty()) {
      sql += ",\n" + constraints;
    }

    sql += "\n);";
    return sql;
  }

  const std::vector<std::string>& bind_params() const { return bind_params_; }
};

/// @brief Generate DROP TABLE SQL statement for a table struct
/// @tparam Table The table struct type
/// @param table_instance An instance of the table
/// @param if_exists Whether to include the IF EXISTS clause
/// @return SQL string to drop the table

template <TableConcept Table>
class drop_table {
public:
  drop_table(const Table& table_instance) : table_instance_(table_instance) {}

  drop_table& if_exists(bool if_exists = true) {
    if_exists_ = if_exists;
    return *this;
  }

  drop_table& cascade(bool cascade = true) {
    cascade_ = cascade;
    return *this;
  }

  drop_table& restrict(bool restrict = true) {
    restrict_ = restrict;
    return *this;
  }

  std::string to_sql() const {
    std::string sql = "DROP TABLE ";

    if (if_exists_) {
      sql += "IF EXISTS ";
    }

    sql += std::string(Table::table_name);

    if (cascade_) {
      sql += " CASCADE";
    }

    if (restrict_) {
      sql += " RESTRICT";
    }

    sql += ";";

    return sql;
  }

  const std::vector<std::string>& bind_params() const { return bind_params_; }

private:
  std::vector<std::string> bind_params_;
  const Table& table_instance_;
  bool if_exists_ = true;
  bool cascade_ = false;
  bool restrict_ = false;
};
}  // namespace relx::schema
