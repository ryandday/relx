#pragma once

#include "column.hpp"
#include <string_view>
#include <type_traits>
#include <array>

namespace relx {
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
    foreign_key() : on_delete_(reference_action::no_action), on_update_(reference_action::no_action) {}
    
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
        
        // Get the table name directly
        std::string ref_table_name = std::string(ref_table_type::table_name);
        
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

/// @brief Represents a composite foreign key constraint with multiple columns
/// @tparam LocalColumnPtrs Pointers to the local columns
/// @tparam ReferencedColumnPtrs Pointers to the referenced columns
template <auto... ColumnPtrs>
class composite_foreign_key {
public:
    /// @brief Default constructor
    explicit composite_foreign_key() 
        : on_delete_(reference_action::no_action), on_update_(reference_action::no_action) {}
    
    /// @brief Constructor with custom actions
    /// @param on_delete Action on delete
    /// @param on_update Action on update
    composite_foreign_key(reference_action on_delete, reference_action on_update) 
        : on_delete_(on_delete), on_update_(on_update) {}
    
    /// @brief Get SQL definition for the composite FOREIGN KEY constraint
    /// @return SQL string defining the constraint
    std::string sql_definition() const {
        // Generate the SQL
        std::string result = "FOREIGN KEY (";
        
        // Add local columns (first half of the parameter pack)
        std::string local_names;
        get_column_names<0, sizeof...(ColumnPtrs) / 2>(local_names);
        result += local_names;
        
        // Add reference columns (second half of the parameter pack)
        result += ") REFERENCES ";
        std::string ref_table_name = get_referenced_table_name();
        result += ref_table_name + " (";
        
        std::string ref_names;
        get_column_names<sizeof...(ColumnPtrs) / 2, sizeof...(ColumnPtrs)>(ref_names);
        result += ref_names;
        
        result += ")";
        
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
    
    // Helper to get comma-separated column names for a range of parameters
    template<size_t Start, size_t End>
    void get_column_names(std::string& names) const {
        get_column_names_impl<Start, End, ColumnPtrs...>(names);
    }
    
    // Implementation of get_column_names that handles the parameter pack
    template<size_t Start, size_t End, auto FirstColumnPtr, auto... RestColumnPtrs>
    void get_column_names_impl(std::string& names) const {
        if constexpr (Start == 0) {
            // First column, start building the name list
            using column_type = typename member_pointer_type<decltype(FirstColumnPtr)>::type;
            names = std::string(column_type::name);
            
            // Process rest of the columns in range
            if constexpr (sizeof...(RestColumnPtrs) > 0 && 1 < End) {
                get_column_names_impl<0, End - 1, RestColumnPtrs...>(names, true);
            }
        } else {
            // Skip this column and continue
            get_column_names_impl<Start - 1, End - 1, RestColumnPtrs...>(names);
        }
    }
    
    // Overload for appending to existing name list
    template<size_t Start, size_t End, auto FirstColumnPtr, auto... RestColumnPtrs>
    void get_column_names_impl(std::string& names, bool append) const {
        if (append) {
            using column_type = typename member_pointer_type<decltype(FirstColumnPtr)>::type;
            names += ", " + std::string(column_type::name);
        }
        
        // Process rest of the columns in range
        if constexpr (sizeof...(RestColumnPtrs) > 0 && 1 < End) {
            get_column_names_impl<0, End - 1, RestColumnPtrs...>(names, append);
        }
    }
    
    // Base case for empty pack
    template<size_t Start, size_t End>
    void get_column_names_impl(std::string& names) const {
        // Do nothing for empty pack
    }
    
    template<size_t Start, size_t End>
    void get_column_names_impl(std::string& names, bool append) const {
        // Do nothing for empty pack
    }
    
    // Helper to get the referenced table name (from the first referenced column)
    std::string get_referenced_table_name() const {
        // Using template specialization to get the right column
        return get_ref_table_name_impl<sizeof...(ColumnPtrs) / 2, ColumnPtrs...>();
    }
    
    // Implementation to get the referenced table name
    template<size_t Index, auto FirstColumnPtr, auto... RestColumnPtrs>
    std::string get_ref_table_name_impl() const {
        if constexpr (Index == 0) {
            // This is the first referenced column
            using table_type = typename member_pointer_class<decltype(FirstColumnPtr)>::type;
            return std::string(table_type::table_name);
        } else {
            // Skip this column and continue
            return get_ref_table_name_impl<Index - 1, RestColumnPtrs...>();
        }
    }
    
    // Base case for empty pack
    template<size_t Index>
    std::string get_ref_table_name_impl() const {
        // Should never reach here if parameters are correct
        return "unknown_table";
    }
    
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

// Helper type alias for splitting column pointers into local and referenced parts
// This preprocesses the arguments to make sure the two packs are equal length
template <typename, typename>
struct fk_impl;

template <auto... LocalColumns, auto... ReferencedColumns>
struct fk_impl<std::tuple<std::integral_constant<decltype(LocalColumns), LocalColumns>...>, 
               std::tuple<std::integral_constant<decltype(ReferencedColumns), ReferencedColumns>...>> {
    using type = std::conditional_t<
        sizeof...(LocalColumns) == 1,
        foreign_key<(LocalColumns, ...), (ReferencedColumns, ...)>,
        composite_foreign_key<LocalColumns..., ReferencedColumns...>
    >;
};

// Helper for getting pack element by index
template <std::size_t N, auto... Args>
struct pack_element;

// Base case: first element
template <auto First, auto... Rest>
struct pack_element<0, First, Rest...> {
    static constexpr auto value = First;
};

// Recursive case: Nth element
template <std::size_t N, auto First, auto... Rest>
struct pack_element<N, First, Rest...> {
    static constexpr auto value = pack_element<N-1, Rest...>::value;
};

/// @brief Helper function to create a foreign key
/// @tparam LocalColumns Local column pointers (first half of Args)
/// @tparam ReferencedColumns Referenced column pointers (second half of Args)
/// @return A foreign key constraint
template <auto... Args>
auto make_fk() {
    constexpr std::size_t total_args = sizeof...(Args);
    constexpr std::size_t half_size = total_args / 2;
    
    static_assert(total_args % 2 == 0, "Number of arguments must be even");
    
    if constexpr (half_size == 1) {
        // Single column foreign key - extract first and second arguments
        constexpr auto first_arg = pack_element<0, Args...>::value;
        constexpr auto second_arg = pack_element<1, Args...>::value;
        
        return foreign_key<first_arg, second_arg>();
    } else {
        // Composite foreign key
        return composite_foreign_key<Args...>();
    }
}

/// @brief Helper function to create a foreign key with reference actions
/// @tparam Args All column pointers (local columns followed by referenced columns)
/// @param on_delete On delete action
/// @param on_update On update action
/// @return A foreign key constraint with the specified actions
template <auto... Args>
auto make_fk(reference_action on_delete, reference_action on_update) {
    constexpr std::size_t total_args = sizeof...(Args);
    constexpr std::size_t half_size = total_args / 2;
    
    static_assert(total_args % 2 == 0, "Number of arguments must be even");
    
    if constexpr (half_size == 1) {
        // Single column foreign key - extract first and second arguments
        constexpr auto first_arg = pack_element<0, Args...>::value;
        constexpr auto second_arg = pack_element<1, Args...>::value;
        
        return foreign_key<first_arg, second_arg>(on_delete, on_update);
    } else {
        // Composite foreign key
        return composite_foreign_key<Args...>(on_delete, on_update);
    }
}

/// @brief Type alias for foreign key using the make_fk helper
/// @tparam Args All column pointers (local followed by referenced)
template <auto... Args>
using fk = decltype(make_fk<Args...>());

} // namespace schema
} // namespace relx
