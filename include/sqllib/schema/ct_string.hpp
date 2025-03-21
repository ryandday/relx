#pragma once

#include <array>
#include <string_view>
#include <algorithm>
#include <ostream>
#include <cstddef>

namespace sqllib {
namespace schema {

/**
 * @brief Compile-time string type that can be used as template non-type parameter in C++20
 * 
 * This implementation allows usage like:
 * template <fixed_string Name> struct table {};
 * table<"users"> users_table;
 */
template <std::size_t N>
struct fixed_string {
    constexpr fixed_string(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }
    
    constexpr fixed_string(const fixed_string&) = default;
    
    char value[N];
    
    constexpr operator std::string_view() const {
        return std::string_view(value, N - 1); // exclude null terminator
    }
    
    constexpr const char* c_str() const {
        return value;
    }
    
    constexpr std::size_t size() const {
        return N - 1; // exclude null terminator
    }
    
    constexpr bool empty() const {
        return size() == 0;
    }
    
    template <std::size_t M>
    constexpr bool operator==(const fixed_string<M>& other) const {
        return std::string_view(*this) == std::string_view(other);
    }
    
    template <std::size_t M>
    constexpr bool operator!=(const fixed_string<M>& other) const {
        return !(*this == other);
    }
};

// Type alias for backward compatibility
template <std::size_t N>
using ct_string = fixed_string<N>;

// Output stream operator
template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const fixed_string<N>& str) {
    return os << std::string_view(str);
}

} // namespace schema

// Namespace alias for convenience
namespace literals {
    // Standard C++11 user-defined literal syntax
    template <char... Chars>
    constexpr auto operator""_fs() {
        constexpr char str[] = {Chars..., '\0'};
        return schema::fixed_string<sizeof...(Chars) + 1>(str);
    }
}

using namespace literals;

} // namespace sqllib
