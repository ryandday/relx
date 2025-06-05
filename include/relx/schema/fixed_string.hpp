#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <ostream>
#include <string_view>

namespace relx {
namespace schema {

/**
 * @brief Compile-time string type that can be used as template non-type parameter in C++20
 *
 * This implementation allows usage like:
 *
 * struct Users {
 *     static constexpr auto table_name = "users";
 *     relx::schema::column<"id", int> id;
 *     relx::schema::column<"name", std::string> name;
 *     relx::schema::column<"email", std::string> email;
 * };
 */
template <std::size_t N>
struct FixedString {
  constexpr FixedString(const char (&str)[N]) : value{} { std::copy_n(str, N, value); }

  constexpr FixedString(const FixedString&) = default;

  char value[N];

  constexpr operator std::string_view() const {
    return std::string_view(value, N - 1);  // exclude null terminator
  }

  constexpr const char* c_str() const { return value; }

  constexpr std::size_t size() const {
    return N - 1;  // exclude null terminator
  }

  constexpr bool empty() const { return size() == 0; }

  template <std::size_t M>
  constexpr bool operator==(const FixedString<M>& other) const {
    return std::string_view(*this) == std::string_view(other);
  }

  template <std::size_t M>
  constexpr bool operator!=(const FixedString<M>& other) const {
    return !(*this == other);
  }
};

// Output stream operator
template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const FixedString<N>& str) {
  return os << std::string_view(str);
}
}
namespace literals {
// Standard C++11 user-defined literal syntax
template <char... Chars>
constexpr auto operator""_fs() {
  constexpr char str[] = {Chars..., '\0'};
  return schema::FixedString<sizeof...(Chars) + 1>(str);
}
}  // namespace literals

}  // namespace relx
