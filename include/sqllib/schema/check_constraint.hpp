#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include <string_view>
#include <string>

namespace sqllib {
namespace schema {

/// @brief Check constraint that accepts a condition string at compile time
/// @details Uses compile-time string literals to define the constraint condition
/// @tparam Condition The SQL condition as a FixedString
/// @tparam Name Optional name for the constraint
template <FixedString Condition, FixedString Name = "">
class check_constraint {
public:
    /// @brief Get SQL definition for the CHECK constraint
    /// @return SQL string defining the constraint
    constexpr std::string sql_definition() const {
        std::string result;
        
        // Add constraint name if provided
        if constexpr (!Name.empty()) {
            result = "CONSTRAINT " + std::string(std::string_view(Name)) + " ";
        }
        
        // Use the condition as-is
        result += "CHECK (" + std::string(std::string_view(Condition)) + ")";
        return result;
    }
    
    /// @brief Get the condition as a string_view
    /// @return The condition string
    constexpr std::string_view condition() const {
        return std::string_view(Condition);
    }
    
    /// @brief Get the name as a string_view
    /// @return The constraint name
    constexpr std::string_view name() const {
        return std::string_view(Name);
    }
};

/// @brief Helper function to create a named check constraint at compile time
/// @tparam Condition The SQL condition
/// @tparam Name The constraint name
/// @return A check constraint with the given condition and name
template <FixedString Condition, FixedString Name>
constexpr auto named_check() {
    return check_constraint<Condition, Name>();
}

/// @brief Helper function to create an unnamed check constraint at compile time
/// @tparam Condition The SQL condition
/// @return A check constraint with the given condition
template <FixedString Condition>
constexpr auto check() {
    return check_constraint<Condition, "">();
}

/// @brief Helper type alias for backward compatibility with older code
template <FixedString Condition, FixedString Name = "">
using table_check_constraint = check_constraint<Condition, Name>;

} // namespace schema
} // namespace sqllib 