#pragma once

#include "value.hpp"

#include <string>
#include <string_view>

namespace relx {
namespace query {

/// @brief Literals namespace for SQL value literals
namespace literals {

/// @brief Automatic conversion from literal integers to Value expressions
/// @param value The integer literal
/// @return A Value expression
inline auto operator""_sql(unsigned long long value) {
  return Value<int>(static_cast<int>(value));
}

/// @brief Automatic conversion from literal floating point numbers to Value expressions
/// @param value The floating point literal
/// @return A Value expression
inline auto operator""_sql(long double value) {
  return Value<double>(static_cast<double>(value));
}

/// @brief Automatic conversion from literal strings to Value expressions
/// @param str The string literal
/// @param len The length of the string
/// @return A Value expression
inline auto operator""_sql(const char* str, std::size_t len) {
  return Value<std::string_view>(std::string_view(str, len));
}

}  // namespace literals

}  // namespace query
}  // namespace relx