#pragma once

#include "column.hpp"
#include "fixed_string.hpp"
#include <boost/pfr.hpp>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <string>
#include <optional>
#include <unordered_set>

namespace sqllib {
namespace schema {

/// @brief Helper to detect column members in a table
template <typename T>
concept is_column = requires {
    T::name;
    T::sql_type;
    std::declval<T>().sql_definition();
};

/// @brief Concept for a database table type
/// @details Requires the type to have a static get_name() method that returns a string_view
template <typename T>
concept TableConcept = requires {
    { T::get_name() } -> std::convertible_to<std::string_view>;
};

/// @brief Helper to detect constraint members in a table
template <typename T>
concept is_constraint = requires(T t) {
    { t.sql_definition() } -> std::convertible_to<std::string>;
} && !is_column<T>; // Ensure a constraint is not also a column

/// @brief Generate SQL column definitions from a table struct using Boost PFR
/// @tparam Table The table struct type
/// @param table_instance An instance of the table
/// @return String containing SQL column definitions
template <TableConcept Table>
std::string collect_column_definitions(const Table& table_instance) {
    std::vector<std::string> column_defs;
    std::unordered_set<std::string> added_columns; // Track added columns by name
    
    boost::pfr::for_each_field(table_instance, [&](const auto& field) {
        if constexpr (is_column<std::remove_cvref_t<decltype(field)>>) {
            // Use column name as key to avoid duplicates
            std::string col_name = std::string(std::remove_cvref_t<decltype(field)>::get_name());
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
    std::unordered_set<std::string> added_constraints; // Track constraints to avoid duplicates
    
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
std::string create_table_sql(const Table& table_instance) {
    std::string sql = "CREATE TABLE IF NOT EXISTS " + 
                     std::string(Table::get_name()) + 
                     " (\n";
    
    // Add column definitions
    sql += collect_column_definitions(table_instance);
    
    // Add constraint definitions
    std::string constraints = collect_constraint_definitions(table_instance);
    if (!constraints.empty()) {
        sql += ",\n" + constraints;
    }
    
    sql += "\n);";
    return sql;
}

} // namespace schema
} // namespace sqllib
