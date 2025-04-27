#pragma once

#include "fixed_string.hpp"
#include "core.hpp"
#include <string_view>
#include <string>
#include <type_traits>

namespace relx {
namespace schema {

/// @brief SQL dialect types for database-specific features
enum class SqlDialect {
    SQLite,     ///< SQLite dialect
    PostgreSQL, ///< PostgreSQL dialect
    MySQL,      ///< MySQL/MariaDB dialect
    Generic     ///< Generic SQL (most compatible)
};
// TODO we will get more compatibility, for now we will just use postgres

/// @brief Column that automatically increments with each new row
/// @tparam Name The name of the column as a string literal
/// @tparam T The C++ type of the column (int, long, etc.)
/// @tparam Dialect The SQL dialect to use for generating SQL
template <FixedString Name, typename T = int, SqlDialect Dialect = SqlDialect::Generic>
class autoincrement {
public:
    /// @brief The C++ type of the column
    using value_type = T;
    
    /// @brief The column name
    static constexpr auto name = Name;
    
    /// @brief The SQL type of the column
    static constexpr auto sql_type = column_traits<T>::sql_type_name;
    
    /// @brief Flag indicating if the column can be NULL (always false for autoincrement)
    static constexpr bool nullable = false;
    
    /// @brief Get the SQL definition of this autoincrement column
    /// @return A string containing the SQL column definition
    std::string sql_definition() const {
        std::string result = std::string(std::string_view(name)) + " ";
        
        if constexpr (Dialect == SqlDialect::PostgreSQL) {
            // PostgreSQL uses SERIAL or BIGSERIAL types
            if constexpr (std::is_same_v<T, int>) {
                result += "SERIAL";
            } else if constexpr (std::is_same_v<T, long> || std::is_same_v<T, long long>) {
                result += "BIGSERIAL";
            } else {
                // Fallback for other types
                result += std::string(std::string_view(sql_type)) + " GENERATED ALWAYS AS IDENTITY";
            }
        } 
        else if constexpr (Dialect == SqlDialect::MySQL) {
            // MySQL uses AUTO_INCREMENT
            result += std::string(std::string_view(sql_type)) + " NOT NULL AUTO_INCREMENT PRIMARY KEY";
        }
        else if constexpr (Dialect == SqlDialect::SQLite) {
            // SQLite uses INTEGER PRIMARY KEY AUTOINCREMENT
            result += "INTEGER PRIMARY KEY AUTOINCREMENT";
        }
        else {
            // Generic SQL (most compatible version)
            result += std::string(std::string_view(sql_type)) + " PRIMARY KEY AUTO_INCREMENT";
        }
        
        return result;
    }
    
    /// @brief Convert a C++ value to SQL string
    /// @param value The value to convert
    /// @return SQL string representation
    static std::string to_sql_string(const T& value) {
        return column_traits<T>::to_sql_string(value);
    }
    
    /// @brief Parse a SQL string to C++ value
    /// @param sql_str The SQL string to parse
    /// @return The C++ value
    static T from_sql_string(const std::string& sql_str) {
        return column_traits<T>::from_sql_string(sql_str);
    }
};

/// @brief Convenience alias for SQLite autoincrement columns
template <FixedString Name, typename T = int>
using sqlite_autoincrement = autoincrement<Name, T, SqlDialect::SQLite>;

/// @brief Convenience alias for PostgreSQL autoincrement columns
template <FixedString Name, typename T = int>
using pg_serial = autoincrement<Name, T, SqlDialect::PostgreSQL>;

/// @brief Convenience alias for MySQL autoincrement columns
template <FixedString Name, typename T = int>
using mysql_auto_increment = autoincrement<Name, T, SqlDialect::MySQL>;

} // namespace schema
} // namespace relx 
