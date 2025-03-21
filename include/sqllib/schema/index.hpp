#pragma once

#include "column.hpp"
#include <string_view>
#include <type_traits>

namespace sqllib {
namespace schema {

/// @brief Index types
enum class index_type {
    normal,
    unique,
    fulltext,
    spatial
};

/// @brief Convert index type to SQL string
/// @param type The index type
/// @return SQL string for the index type
constexpr std::string_view index_type_to_string(index_type type) {
    switch (type) {
        case index_type::unique:   return "UNIQUE ";
        case index_type::fulltext: return "FULLTEXT ";
        case index_type::spatial:  return "SPATIAL ";
        case index_type::normal:   // Fall through
        default:                   return "";
    }
}

/// @brief Represents an index on a table
/// @tparam ColumnPtr Pointer to the column member
template <auto ColumnPtr>
class index {
public:
    /// @brief Default constructor - creates a normal index
    index() : type_(index_type::normal) {}
    
    /// @brief Constructor with specific index type
    /// @param type The type of index to create
    explicit index(index_type type) : type_(type) {}
    
    /// @brief Get SQL statement for creating the index
    /// @return SQL CREATE INDEX statement
    std::string create_index_sql() const {
        // Extract the column name from the pointer
        using table_type = typename member_pointer_class<decltype(ColumnPtr)>::type;
        using column_type = typename member_pointer_type<decltype(ColumnPtr)>::type;
        
        // Use the static getter methods
        std::string table_name = std::string(table_type::get_name());
        std::string column_name = std::string(column_type::get_name());
        
        std::string index_name = table_name + "_" + column_name + "_idx";
        
        std::string result = "CREATE ";
        result += std::string(index_type_to_string(type_));
        result += "INDEX " + index_name + " ON ";
        result += table_name + " (" + column_name + ")";
        
        return result;
    }
    
private:
    index_type type_;
    
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

/// @brief Represents a composite index on multiple columns
/// @tparam ColumnPtrs Pointers to the columns that form the index
template <auto... ColumnPtrs>
class composite_index {
public:
    /// @brief Default constructor - creates a normal index
    composite_index() : type_(index_type::normal) {}
    
    /// @brief Constructor with specific index type
    /// @param type The type of index to create
    explicit composite_index(index_type type) : type_(type) {}
    
    /// @brief Get SQL statement for creating the composite index
    /// @return SQL CREATE INDEX statement
    std::string create_index_sql() const {
        // This is a placeholder - actual implementation would need reflection
        std::string result = "CREATE ";
        result += std::string(index_type_to_string(type_));
        result += "INDEX ...";
        
        return result;
    }
    
private:
    index_type type_;
};

} // namespace schema
} // namespace sqllib
