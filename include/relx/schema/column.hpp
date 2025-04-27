#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include <string_view>
#include <type_traits>
#include <optional>
#include <tuple>

namespace relx {
namespace schema {

/// @brief UNIQUE constraint
struct unique {
    static std::string to_sql() {
        return " UNIQUE";
    }
};

/// @brief PRIMARY KEY constraint
struct primary_key {
    static std::string to_sql() {
        return " PRIMARY KEY";
    }
};

/// @brief AUTOINCREMENT constraint
struct autoincrement {
    static std::string to_sql() {
        // SQLite syntax
        return " AUTOINCREMENT";
    }
};

/// @brief SERIAL modifier (PostgreSQL's equivalent to AUTOINCREMENT)
struct serial {
    static std::string to_sql() {
        // PostgreSQL syntax
        return " SERIAL";
    }
};

/// @brief CHECK constraint
template <FixedString Expr>
struct check {
    static constexpr auto expr = Expr;
    
    static std::string to_sql() {
        return " CHECK(" + std::string(std::string_view(expr)) + ")";
    }
};

/// @brief REFERENCES constraint for foreign keys
template <FixedString Table, FixedString Column>
struct references {
    static constexpr auto table = Table;
    static constexpr auto column = Column;
    
    static std::string to_sql() {
        return " REFERENCES " + std::string(std::string_view(table)) + 
               "(" + std::string(std::string_view(column)) + ")";
    }
};

/// @brief ON DELETE action for foreign keys
template <FixedString Action>
struct on_delete {
    static constexpr auto action = Action;
    
    static std::string to_sql() {
        return " ON DELETE " + std::string(std::string_view(action));
    }
};

/// @brief ON UPDATE action for foreign keys
template <FixedString Action>
struct on_update {
    static constexpr auto action = Action;
    
    static std::string to_sql() {
        return " ON UPDATE " + std::string(std::string_view(action));
    }
};

/// @brief DEFAULT value for non-string values
template <auto Value>
struct default_value {
    using value_type = decltype(Value);
    
    static constexpr value_type value = Value;
    
    static std::string to_sql() {
        if constexpr (std::is_same_v<value_type, bool>) {
            // For boolean values, use 1/0 in SQL
            return " DEFAULT " + std::string(value ? "1" : "0");
        } else if constexpr (std::is_integral_v<value_type> || std::is_floating_point_v<value_type>) {
            // For numeric types, convert to string
            return " DEFAULT " + std::to_string(value);
        } else {
            // Fall back to generic string conversion
            return " DEFAULT " + std::to_string(value);
        }
    }
};

/// @brief DEFAULT value for string literals
template <FixedString Value, bool IsLiteral = false>
struct string_default {
    static constexpr auto value = Value;
    
    static std::string to_sql() {
        if constexpr (IsLiteral) {
            // SQL function or literal that should not be quoted
            return " DEFAULT " + std::string(std::string_view(value));
        } else {
            // Normal string that should be quoted
            return " DEFAULT '" + std::string(std::string_view(value)) + "'";
        }
    }
};

/// @brief NULL default for optional types
struct null_default {
    static std::string to_sql() {
        return " DEFAULT NULL";
    }
};

/// @brief Helper to apply all column modifiers to a SQL definition
template <typename... Modifiers>
static std::string apply_modifiers() {
    std::string result;
    (result += ... += Modifiers::to_sql());
    return result;
}

// Type trait to detect default_value specialization
template <typename TypeParam>
struct is_default_value_specialization : std::false_type {};

template <auto Value>
struct is_default_value_specialization<default_value<Value>> : std::true_type {
    using value_type = decltype(Value);
};

// Type trait to detect string_default specialization
template <typename TypeParam>
struct is_string_default_specialization : std::false_type {};

template <FixedString Value, bool IsLiteral>
struct is_string_default_specialization<string_default<Value, IsLiteral>> : std::true_type {};

/// @brief Represents a column in a database table
/// @tparam Name The name of the column as a string literal
/// @tparam T The C++ type of the column
/// @tparam Modifiers Additional column modifiers (UNIQUE, PRIMARY KEY, etc.)
template <FixedString Name, typename T, typename... Modifiers>
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
    
    /// @brief Get the SQL definition of this column
    /// @return A string containing the SQL column definition
    std::string sql_definition() const {
        std::string result = std::string(std::string_view(name)) + " " + std::string(std::string_view(sql_type));
        
        // Add NOT NULL if not nullable
        if (!nullable) {
            result += " NOT NULL";
        }
        
        // Apply all modifiers
        result += apply_modifiers<Modifiers...>();
        
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
    std::optional<T> get_default_value() const {
        return find_default_value<T, Modifiers...>();
    }
    
private:
    // Helper to find default value in the modifiers
    template <typename ValueT, typename First, typename... Rest>
    static std::optional<ValueT> find_default_value() {
        if constexpr (is_default_value_type<First, ValueT>()) {
            return extract_default_value<First, ValueT>();
        } else if constexpr (sizeof...(Rest) > 0) {
            return find_default_value<ValueT, Rest...>();
        } else {
            return std::nullopt;
        }
    }
    
    template <typename ValueT>
    static std::optional<ValueT> find_default_value() {
        return std::nullopt;
    }
    
    // Check if a type is a default value type
    template <typename Mod, typename ValueT>
    static constexpr bool is_default_value_type() {
        if constexpr (std::is_same_v<Mod, null_default>) {
            return column_traits<ValueT>::nullable;
        } else if constexpr (is_default_value_specialization<Mod>::value) {
            using DefaultValueType = typename is_default_value_specialization<Mod>::value_type;
            return std::is_same_v<DefaultValueType, ValueT> || 
                   std::is_convertible_v<DefaultValueType, ValueT>;
        } else if constexpr (is_string_default_specialization<Mod>::value) {
            return std::is_convertible_v<std::string_view, ValueT>;
        } else {
            return false;
        }
    }
    
    // Extract the default value from a modifier
    template <typename Mod, typename ValueT>
    static std::optional<ValueT> extract_default_value() {
        if constexpr (std::is_same_v<Mod, null_default>) {
            if constexpr (column_traits<ValueT>::nullable) {
                return std::nullopt;
            }
        } else if constexpr (is_default_value_specialization<Mod>::value) {
            return static_cast<ValueT>(Mod::value);
        } else if constexpr (is_string_default_specialization<Mod>::value) {
            return static_cast<ValueT>(std::string_view(Mod::value));
        }
        return std::nullopt;
    }
};

// Specialization for std::optional columns (nullable)
template <FixedString Name, typename T, typename... Modifiers>
class column<Name, std::optional<T>, Modifiers...> {
public:
    using value_type = std::optional<T>;
    using base_type = T;
    
    static constexpr auto name = Name;
    static constexpr auto sql_type = column_traits<T>::sql_type_name;
    static constexpr bool nullable = true;
    
    std::string sql_definition() const {
        std::string result = std::string(std::string_view(name)) + " " + std::string(std::string_view(sql_type));
        
        // No NOT NULL constraint for optional columns
        
        // Apply all modifiers
        result += apply_modifiers<Modifiers...>();
        
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
    
    /// @brief Get the default value if set
    /// @return Optional containing the default value or empty if not set
    std::optional<std::optional<T>> get_default_value() const {
        return find_default_value<std::optional<T>, Modifiers...>();
    }
    
private:
    // Helper to find default value in the modifiers
    template <typename ValueT, typename First, typename... Rest>
    static std::optional<ValueT> find_default_value() {
        if constexpr (is_default_value_type<First, ValueT>()) {
            return extract_default_value<First, ValueT>();
        } else if constexpr (sizeof...(Rest) > 0) {
            return find_default_value<ValueT, Rest...>();
        } else {
            return std::nullopt;
        }
    }
    
    template <typename ValueT>
    static std::optional<ValueT> find_default_value() {
        return std::nullopt;
    }
    
    // Check if a type is a default value type
    template <typename Mod, typename ValueT>
    static constexpr bool is_default_value_type() {
        if constexpr (std::is_same_v<Mod, null_default>) {
            return true;  // null_default is valid for std::optional<T>
        } else if constexpr (is_default_value_specialization<Mod>::value) {
            using DefaultValueType = typename is_default_value_specialization<Mod>::value_type;
            using OptionalBaseType = typename ValueT::value_type;
            return std::is_same_v<DefaultValueType, OptionalBaseType> || 
                   std::is_convertible_v<DefaultValueType, OptionalBaseType>;
        } else if constexpr (is_string_default_specialization<Mod>::value) {
            using OptionalBaseType = typename ValueT::value_type;
            return std::is_convertible_v<std::string_view, OptionalBaseType>;
        } else {
            return false;
        }
    }
    
    // Extract the default value from a modifier
    template <typename Mod, typename ValueT>
    static std::optional<ValueT> extract_default_value() {
        using OptionalBaseType = typename ValueT::value_type;
        
        if constexpr (std::is_same_v<Mod, null_default>) {
            return ValueT(std::nullopt);
        } else if constexpr (is_default_value_specialization<Mod>::value) {
            return ValueT(static_cast<OptionalBaseType>(Mod::value));
        } else if constexpr (is_string_default_specialization<Mod>::value) {
            return ValueT(static_cast<OptionalBaseType>(std::string_view(Mod::value)));
        }
        return std::nullopt;
    }
};

} // namespace schema
} // namespace relx
