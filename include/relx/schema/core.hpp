#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <optional>
#include "fixed_string.hpp"

namespace relx {

/// @brief Contains schema definition components
namespace schema {

/// @brief Metadata for a SQL column
template <typename T>
struct column_traits {
    /// @brief The SQL type name for this C++ type
    static constexpr auto sql_type_name = "TEXT";
    
    /// @brief Whether this type can be NULL
    static constexpr bool nullable = false;
    
    /// @brief Convert a C++ value to a SQL string representation
    static std::string to_sql_string(const T& value);
    
    /// @brief Parse a SQL string representation to a C++ value
    static T from_sql_string(const std::string& value);
};

// Specializations for common types

template <>
struct column_traits<int> {
    static constexpr auto sql_type_name = "INTEGER";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const int& value) {
        return std::to_string(value);
    }
    
    static int from_sql_string(const std::string& value) {
        return std::stoi(value);
    }
};

template <>
struct column_traits<double> {
    static constexpr auto sql_type_name = "REAL";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const double& value) {
        return std::to_string(value);
    }
    
    static double from_sql_string(const std::string& value) {
        return std::stod(value);
    }
};

template <>
struct column_traits<std::string> {
    static constexpr auto sql_type_name = "TEXT";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const std::string& value) {
        // Escape single quotes by doubling them
        std::string escaped = value;
        size_t pos = 0;
        while ((pos = escaped.find('\'', pos)) != std::string::npos) {
            escaped.insert(pos, 1, '\'');
            pos += 2;
        }
        return "'" + escaped + "'";
    }
    
    static std::string from_sql_string(const std::string& value) {
        // Remove surrounding quotes if present
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
            std::string unquoted = value.substr(1, value.size() - 2);
            // Unescape doubled single quotes
            size_t pos = 0;
            while ((pos = unquoted.find("''", pos)) != std::string::npos) {
                unquoted.erase(pos, 1);
                pos += 1;
            }
            return unquoted;
        }
        return value;
    }
};

template <>
struct column_traits<bool> {
    static constexpr auto sql_type_name = "BOOLEAN";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const bool& value) {
        return value ? "1" : "0";
    }
    
    static bool from_sql_string(const std::string& value) {
        return value == "1" || value == "true" || value == "TRUE";
    }
};

template <>
struct column_traits<float> {
    static constexpr auto sql_type_name = "REAL";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const float& value) {
        return std::to_string(value);
    }
    
    static float from_sql_string(const std::string& value) {
        return std::stof(value);
    }
};

template <>
struct column_traits<long> {
    static constexpr auto sql_type_name = "INTEGER";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const long& value) {
        return std::to_string(value);
    }
    
    static long from_sql_string(const std::string& value) {
        return std::stol(value);
    }
};

template <>
struct column_traits<long long> {
    static constexpr auto sql_type_name = "INTEGER";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const long long& value) {
        return std::to_string(value);
    }
    
    static long long from_sql_string(const std::string& value) {
        return std::stoll(value);
    }
};

// Add specialization for std::optional types
template <typename T>
struct column_traits<std::optional<T>> {
    static constexpr auto sql_type_name = column_traits<T>::sql_type_name;
    static constexpr bool nullable = true;
    
    static std::string to_sql_string(const std::optional<T>& value) {
        if (value) {
            return column_traits<T>::to_sql_string(*value);
        } else {
            return "NULL";
        }
    }
    
    static std::optional<T> from_sql_string(const std::string& value) {
        if (value == "NULL") {
            return std::nullopt;
        }
        return column_traits<T>::from_sql_string(value);
    }
};

// Specialization for std::nullopt_t
template <>
struct column_traits<std::nullopt_t> {
    static constexpr auto sql_type_name = "TEXT";
    static constexpr bool nullable = true;

    static std::string to_sql_string(const std::nullopt_t&) {
        return "NULL";
    }

    static std::nullopt_t from_sql_string(const std::string& value) {
        return std::nullopt;
    }
};  

// Concept for defining what is a valid SQL column type
template <typename T>
concept ColumnTypeConcept = requires {
    { column_traits<T>::sql_type_name } -> std::convertible_to<std::string_view>;
    { column_traits<T>::nullable } -> std::convertible_to<bool>;
    { column_traits<T>::to_sql_string(std::declval<T>()) } -> std::convertible_to<std::string>;
    { column_traits<T>::from_sql_string(std::declval<std::string>()) } -> std::convertible_to<T>;
};

} // namespace schema
} // namespace relx
