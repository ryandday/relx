#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include <string_view>
#include <type_traits>

namespace sqllib {
namespace schema {

/// @brief Represents a column in a database table
/// @tparam Name The name of the column as a string literal
/// @tparam T The C++ type of the column
template <FixedString Name, ColumnTypeConcept T>
class column {
public:
    /// @brief The C++ type of the column
    using value_type = T;
    
    /// @brief The column name
    static constexpr auto name = Name;
    
    /// @brief Static function to get the column name
    /// @return The column name as a string_view
    static constexpr std::string_view get_name() {
        return std::string_view(name);
    }
    
    /// @brief The SQL type of the column
    static constexpr auto sql_type = column_traits<T>::sql_type_name;
    
    /// @brief Flag indicating if the column can be NULL
    static constexpr bool nullable = column_traits<T>::nullable;
    
    /// @brief Default constructor
    column() = default;
    
    /// @brief Get the SQL definition of this column
    /// @return A string containing the SQL column definition
    std::string sql_definition() const {
        std::string result = std::string(std::string_view(name)) + " " + std::string(std::string_view(sql_type));
        if (!nullable) {
            result += " NOT NULL";
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

/// @brief Helper to make a column nullable
/// @tparam Name Column name
/// @tparam T Column type
template <FixedString Name, ColumnTypeConcept T>
class nullable_column : public column<Name, T> {
public:
    /// @brief Overrides the nullable trait to true
    static constexpr bool nullable = true;
    
    /// @brief Get the SQL definition with NULL support
    /// @return A string containing the SQL column definition
    std::string sql_definition() const {
        return std::string(std::string_view(this->name)) + " " + std::string(std::string_view(this->sql_type));
    }
};

} // namespace schema
} // namespace sqllib
