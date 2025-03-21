#pragma once

#include "column.hpp"
#include <string_view>
#include <type_traits>

namespace sqllib {
namespace schema {

/// @brief Represents a primary key constraint on a table
/// @tparam ColumnPtr Pointer to the column member
template <auto ColumnPtr>
class primary_key {
public:
    /// @brief Default constructor
    primary_key() = default;
    
    /// @brief Get SQL definition for the PRIMARY KEY constraint
    /// @return SQL string defining the constraint
    std::string sql_definition() const {
        // Extract the column name from the pointer
        using table_type = typename member_pointer_class<decltype(ColumnPtr)>::type;
        using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;
        
        return "PRIMARY KEY (" + std::string(column_type::get_name()) + ")";
    }
    
private:
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

/// @brief Represents a primary key constraint with multiple columns (composite key)
/// @tparam ColumnPtrs Pointers to the columns that form the primary key
template <auto... ColumnPtrs>
class composite_primary_key {
public:
    /// @brief Default constructor
    composite_primary_key() = default;
    
    /// @brief Get SQL definition for the composite PRIMARY KEY constraint
    /// @return SQL string defining the constraint
    std::string sql_definition() const {
        std::string result = "PRIMARY KEY (";
        result += get_column_names();
        result += ")";
        return result;
    }
    
private:
    // Helper to get comma-separated column names
    std::string get_column_names() const {
        std::string names;
        // Fold expression to process all column pointers
        // This is a placeholder - actual implementation would need reflection
        return names;
    }
};

} // namespace schema
} // namespace sqllib
