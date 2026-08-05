#pragma once

#include "../bind_param.hpp"
#include "column.hpp"
#include "fixed_string.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

/// @brief Generate CREATE TABLE SQL statement for a table struct
/// @tparam Table The table struct type
/// @param table_instance An instance of the table
/// @return SQL string to create the table
template <TableConcept Table>
class create_table {
private:
  const Table& table_instance_;
  bool if_not_exists_ = false;

  std::vector<bind_param> bind_params_;

public:
  create_table(const Table& table_instance) : table_instance_(table_instance) {}

  create_table& if_not_exists(bool if_not_exists = true) {
    if_not_exists_ = if_not_exists;
    return *this;
  }

  std::string to_sql() const {
    std::string sql = "CREATE TABLE ";
    if (if_not_exists_) {
      sql += "IF NOT EXISTS ";
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

  const std::vector<bind_param>& bind_params() const { return bind_params_; }
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

  const std::vector<bind_param>& bind_params() const { return bind_params_; }

private:
  std::vector<bind_param> bind_params_;
  const Table& table_instance_;
  bool if_exists_ = true;
  bool cascade_ = false;
  bool restrict_ = false;
};
}  // namespace relx::schema
