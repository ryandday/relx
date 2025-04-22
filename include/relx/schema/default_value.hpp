#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include <string_view>
#include <type_traits>
#include <optional>
#include <concepts>

namespace relx {
namespace schema {

/// @brief Marker type to represent SQL literals (like CURRENT_TIMESTAMP) that don't have a direct C++ representation
struct sql_literal_base {
    constexpr sql_literal_base(std::string_view literal) : value(literal) {}
    std::string_view value;
};

// Define specific SQL literals
struct current_timestamp_t : sql_literal_base {
    constexpr current_timestamp_t() : sql_literal_base("CURRENT_TIMESTAMP") {}
    static constexpr std::string_view name = "CURRENT_TIMESTAMP";
};

struct current_date_t : sql_literal_base {
    constexpr current_date_t() : sql_literal_base("CURRENT_DATE") {}
    static constexpr std::string_view name = "CURRENT_DATE";
};

struct current_time_t : sql_literal_base {
    constexpr current_time_t() : sql_literal_base("CURRENT_TIME") {}
    static constexpr std::string_view name = "CURRENT_TIME";
};

// Global instances of SQL literals
inline constexpr current_timestamp_t current_timestamp{};
inline constexpr current_date_t current_date{};
inline constexpr current_time_t current_time{};

/// @brief Type-safe default value with automatic type deduction
/// @tparam V The literal value (automatically deduces the type)
template <auto V>
class DefaultValue {
public:
    // Deduce the value type from the template parameter
    using value_type = decltype(V);
    
    // Store the value
    static constexpr value_type value = V;
    
    /// @brief Get the SQL definition part for the default value
    /// @return A string containing the DEFAULT clause
    static std::string sql_definition() {
        if constexpr (std::is_same_v<value_type, bool>) {
            // For boolean values, use 1/0 in SQL
            return " DEFAULT " + std::string(value ? "1" : "0");
        } else if constexpr (std::is_integral_v<value_type> || std::is_floating_point_v<value_type>) {
            // For numeric types, convert to string
            return " DEFAULT " + std::to_string(value);
        } else if constexpr (std::is_convertible_v<value_type, std::string_view>) {
            // For string literals
            std::string_view value_view = std::string_view(value);
            if ((value_view.front() != '\'' || value_view.back() != '\'') && 
                value_view.find('(') == std::string_view::npos) {
                return " DEFAULT '" + std::string(value_view) + "'";
            }
            return " DEFAULT " + std::string(value_view);
        } else if constexpr (std::is_base_of_v<sql_literal_base, value_type>) {
            // Handle SQL literal types
            return " DEFAULT " + std::string(value.value);
        } else {
            // Fall back to generic string conversion
            return " DEFAULT " + std::to_string(value);
        }
    }
    
    /// @brief Get the default value
    /// @return The value wrapped in an optional
    static std::optional<value_type> parse_value() {
        return value;
    }
};

/// @brief NULL default for optional types
class null_default {
public:
    template <typename T>
    static std::optional<T> parse_value() {
        return std::nullopt;
    }
    
    static std::string sql_definition() {
        return " DEFAULT NULL";
    }
};

// Helper concept to check if T is a default value
template <typename T>
concept DefaultValueConcept = requires {
    typename T::value_type;
    { T::sql_definition() } -> std::convertible_to<std::string>;
    requires std::is_same_v<decltype(T::parse_value()), std::optional<typename T::value_type>> ||
             requires(T t) { { t.parse_value() } -> std::convertible_to<std::optional<typename T::value_type>>; };
};

} // namespace schema
} // namespace relx 