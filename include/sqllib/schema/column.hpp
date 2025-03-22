#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include "default_value.hpp"
#include <string_view>
#include <type_traits>
#include <optional>

namespace sqllib {
namespace schema {

/// @brief Represents a column in a database table
/// @tparam Name The name of the column as a string literal
/// @tparam T The C++ type of the column
/// @tparam DefaultValueType Optional default value specification
template <FixedString Name, typename T, typename DefaultValueType = void>
class column {
public:
    /// @brief The C++ type of the column
    using value_type = T;
    
    /// @brief The column name
    static constexpr auto name = Name;
    
    /// @brief The SQL type of the column
    static constexpr auto sql_type = column_traits<T>::sql_type_name;
    
    /// @brief Flag indicating if the column can be NULL
    static constexpr bool nullable = column_traits<T>::nullable;
    
    /// @brief Flag indicating if the column has a default value
    static constexpr bool has_default = !std::is_void_v<DefaultValueType>;
    
    /// @brief Get the SQL definition of this column
    /// @return A string containing the SQL column definition
    std::string sql_definition() const {
        std::string result = std::string(std::string_view(name)) + " " + std::string(std::string_view(sql_type));
        
        // Add NOT NULL if not nullable
        if (!nullable) {
            result += " NOT NULL";
        }
        
        // Add DEFAULT if specified
        if constexpr (has_default) {
            result += DefaultValueType::sql_definition();
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
    
    /// @brief Get the default value if set
    /// @return Optional containing the default value or empty if not set
    static std::optional<T> get_default_value() {
        if constexpr (has_default) {
            if constexpr (!std::is_same_v<typename DefaultValueType::value_type, T> && 
                          !std::is_convertible_v<typename DefaultValueType::value_type, T>) {
                // If types don't match exactly, try to convert
                auto default_val = DefaultValueType::parse_value();
                if (default_val) {
                    try {
                        return static_cast<T>(*default_val);
                    } catch (...) {
                        return std::nullopt;
                    }
                }
                return std::nullopt;
            } else {
                // Types match, return directly
                return DefaultValueType::parse_value();
            }
        } else {
            return std::nullopt;
        }
    }
};

// Specialization for std::optional columns (nullable)
template <FixedString Name, typename T, typename DefaultValueType>
class column<Name, std::optional<T>, DefaultValueType> {
public:
    using value_type = std::optional<T>;
    using base_type = T;
    
    static constexpr auto name = Name;
    static constexpr auto sql_type = column_traits<T>::sql_type_name;
    static constexpr bool nullable = true;
    static constexpr bool has_default = !std::is_void_v<DefaultValueType>;
    
    std::string sql_definition() const {
        std::string result = std::string(std::string_view(name)) + " " + std::string(std::string_view(sql_type));
        
        // No NOT NULL constraint for optional columns
        
        // Add DEFAULT if specified
        if constexpr (has_default) {
            result += DefaultValueType::sql_definition();
        }
        
        return result;
    }
    
    static std::string to_sql_string(const std::optional<T>& value) {
        if (value.has_value()) {
            return column_traits<T>::to_sql_string(*value);
        } else {
            return "NULL";
        }
    }
    
    static std::optional<T> from_sql_string(const std::string& sql_str) {
        if (sql_str == "NULL") {
            return std::nullopt;
        }
        return column_traits<T>::from_sql_string(sql_str);
    }
    
    static std::optional<std::optional<T>> get_default_value() {
        if constexpr (has_default) {
            if constexpr (std::is_same_v<DefaultValueType, null_default>) {
                return std::optional<T>{std::nullopt};
            } else {
                auto value = DefaultValueType::parse_value();
                if (value) {
                    // Try to convert if needed
                    if constexpr (!std::is_same_v<typename DefaultValueType::value_type, T> && 
                                  !std::is_convertible_v<typename DefaultValueType::value_type, T>) {
                        try {
                            return std::optional<T>(static_cast<T>(*value));
                        } catch (...) {
                            return std::nullopt;
                        }
                    } else {
                        return std::optional<T>(*value);
                    }
                }
            }
        }
        return std::nullopt;
    }
};

// For backward compatibility only
template <FixedString Name, typename T>
using nullable_column = column<Name, std::optional<T>>;

} // namespace schema
} // namespace sqllib
