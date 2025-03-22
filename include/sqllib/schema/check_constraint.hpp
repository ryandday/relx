#pragma once

#include "core.hpp"
#include "fixed_string.hpp"
#include <string_view>
#include <string>

namespace sqllib {
namespace schema {

/// @brief Simple check constraint that accepts a condition string
/// @details This simplified constraint doesn't perform type checking and
///          lets the SQL engine handle validation
class check_constraint {
public:
    /// @brief Constructor with condition string
    /// @param condition The SQL condition as a string
    /// @param name Optional name for the constraint
    explicit check_constraint(std::string condition, std::string name = "")
        : condition_(std::move(condition)), name_(std::move(name)) {}
    
    /// @brief Get SQL definition for the CHECK constraint
    /// @return SQL string defining the constraint
    std::string sql_definition() const {
        std::string result;
        
        // Add constraint name if provided
        if (!name_.empty()) {
            result = "CONSTRAINT " + name_ + " ";
        }
        
        // Use the condition as-is
        result += "CHECK (" + condition_ + ")";
        return result;
    }

private:
    std::string condition_; ///< The SQL condition
    std::string name_;      ///< Optional constraint name
};

/// @brief Helper function to create a named check constraint
/// @param condition The SQL condition
/// @param name The constraint name
/// @return A check constraint with the given condition and name
inline check_constraint named_check(std::string condition, std::string name) {
    return check_constraint(std::move(condition), std::move(name));
}

// For backward compatibility with fixed strings
template <FixedString Condition, FixedString Name = FixedString("")>
check_constraint make_check_constraint() {
    return check_constraint(std::string(std::string_view(Condition)), 
                           std::string(std::string_view(Name)));
}

/// @brief Helper type alias for backward compatibility
template <FixedString Condition, FixedString Name = FixedString("")>
using table_check_constraint = check_constraint;

} // namespace schema
} // namespace sqllib 