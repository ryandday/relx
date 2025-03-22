#pragma once

#include "column.hpp"
#include "fixed_string.hpp"
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sqllib {
namespace schema {

/// @brief Helper to detect column members in a table
template <typename T>
concept is_column = requires {
    T::name;
    T::sql_type;
    std::declval<T>().sql_definition();
};

/// @brief Represents a database table
/// @tparam Name The table name as a string literal
template <FixedString Name>
class table {
public:
    /// @brief The name of the table
    static constexpr auto name = Name;
    
    /// @brief Static function to get the table name
    /// @return The table name as a string_view
    static constexpr std::string_view get_name() {
        return std::string_view(name);
    }
    
    /// @brief Get SQL CREATE TABLE statement for this table
    /// @return SQL string to create the table
    std::string create_table_sql() const {
        std::string sql = "CREATE TABLE IF NOT EXISTS " + std::string(std::string_view(name)) + " (\n";
        
        // Add column definitions
        sql += collect_column_definitions();
        
        // Add constraint definitions
        std::string constraints = collect_constraint_definitions();
        if (!constraints.empty()) {
            sql += ",\n" + constraints;
        }
        
        sql += "\n);";
        return sql;
    }

    /// @brief Implementation-specific method to collect column definitions
    /// This needs to be implemented in derived classes
    virtual std::string collect_column_definitions() const {
        return "";
    }
    
    /// @brief Implementation-specific method to collect constraint definitions
    /// This needs to be implemented in derived classes
    virtual std::string collect_constraint_definitions() const {
        return "";
    }
    
private:
    /// @brief Helper to recursively collect column definitions
    /// @param obj The object to inspect
    /// @param column_defs Vector to store column definitions
    template <typename T>
    static void collect_columns_impl(const T& obj, std::vector<std::string>& column_defs) {
        // Use reflection-like inspection of the class (C++20 way)
        for_each_member(obj, [&](auto& member) {
            if constexpr (is_column<std::remove_cvref_t<decltype(member)>>) {
                column_defs.push_back(member.sql_definition());
            }
        });
    }
    
    /// @brief Helper to iterate through each member of an object (simplified reflection)
    /// @tparam T Object type
    /// @tparam F Function type
    /// @param obj The object to inspect
    /// @param f Function to apply to each member
    template <typename T, typename F>
    static void for_each_member(const T& obj, F&& f) {
        // This is a placeholder for C++20 reflection
        // In real implementation, we would need to use intrusive reflection
        // or wait for C++23/26 reflection features
        
        // For now, we'll have to manually implement this in derived classes
    }
};

} // namespace schema
} // namespace sqllib
