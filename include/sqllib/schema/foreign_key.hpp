#pragma once

#include "column.hpp"
#include <string_view>
#include <type_traits>

namespace sqllib {
namespace schema {

/// @brief Foreign key actions
enum class reference_action {
    cascade,
    restrict,
    set_null,
    set_default,
    no_action
};

/// @brief Convert reference action to SQL string
/// @param action The action to convert
/// @return SQL string for the reference action
constexpr std::string_view reference_action_to_string(reference_action action) {
    switch (action) {
        case reference_action::cascade:     return "CASCADE";
        case reference_action::restrict:    return "RESTRICT";
        case reference_action::set_null:    return "SET NULL";
        case reference_action::set_default: return "SET DEFAULT";
        case reference_action::no_action:   return "NO ACTION";
        default:                            return "NO ACTION";
    }
}

/// @brief Represents a foreign key constraint on a table
/// @tparam LocalColumnPtr Pointer to the local column
/// @tparam ReferencedColumnPtr Pointer to the referenced column
template <auto LocalColumnPtr, auto ReferencedColumnPtr>
class foreign_key {
public:
    /// @brief Default constructor
    explicit foreign_key() : on_delete_(reference_action::no_action), on_update_(reference_action::no_action) {}
    
    /// @brief Constructor with custom actions
    /// @param on_delete Action on delete
    /// @param on_update Action on update
    foreign_key(reference_action on_delete, reference_action on_update) 
        : on_delete_(on_delete), on_update_(on_update) {}
    
    /// @brief Get SQL definition for the FOREIGN KEY constraint
    /// @return SQL string defining the constraint
    std::string sql_definition() const {
        // Extract column information
        using local_table_type = typename member_pointer_class<decltype(LocalColumnPtr)>::type;
        using local_column_type = typename member_pointer_type<decltype(LocalColumnPtr)>::type;
        
        using ref_table_type = typename member_pointer_class<decltype(ReferencedColumnPtr)>::type;
        using ref_column_type = typename member_pointer_type<decltype(ReferencedColumnPtr)>::type;
        
        // Get the table name using the appropriate method
        std::string ref_table_name;
        if constexpr (requires { { ref_table_type::name } -> std::convertible_to<std::string_view>; }) {
            ref_table_name = std::string(ref_table_type::name);
        } else {
            ref_table_name = std::string(ref_table_type::table_name);
        }
        
        // Generate the SQL using the static members
        std::string result = "FOREIGN KEY (" + std::string(local_column_type::name) + ") ";
        result += "REFERENCES " + ref_table_name + " (";
        result += std::string(ref_column_type::name) + ")";
        
        // Add ON DELETE and ON UPDATE clauses
        if (on_delete_ != reference_action::no_action) {
            result += " ON DELETE " + std::string(reference_action_to_string(on_delete_));
        }
        
        if (on_update_ != reference_action::no_action) {
            result += " ON UPDATE " + std::string(reference_action_to_string(on_update_));
        }
        
        return result;
    }
    
private:
    reference_action on_delete_ = reference_action::no_action;
    reference_action on_update_ = reference_action::no_action;
    
    // Helper to extract the class type from a member pointer
    template <typename T>
    struct member_pointer_class;
    
    template <typename C, typename T>
    struct member_pointer_class<T C::*> {
        using type = C;
    };
    
    // Helper to extract the member type from a member pointer
    template <typename T>
    struct member_pointer_type;
    
    template <typename C, typename T>
    struct member_pointer_type<T C::*> {
        using type = T;
    };
};

} // namespace schema
} // namespace sqllib
