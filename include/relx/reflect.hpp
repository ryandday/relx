#pragma once

#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202603L
#error "relx requires C++26 reflection (P2996). Build with GCC 16.1+ and -std=c++26 -freflection."
#endif

/// @brief Compile-time struct member iteration built on C++26 reflection (P2996).
/// Replaces the previous Boost.PFR machinery.
namespace relx::refl {

// clang-format off

/// @brief All non-static data members of T as a static array of std::meta::info
template <typename T>
consteval auto member_array() {
  return std::define_static_array(
      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
}

/// @brief Number of non-static data members of T
template <typename T>
consteval std::size_t field_count() {
  return member_array<T>().size();
}

/// @brief Invoke fn(field_ref) for each non-static data member of obj
template <typename T, typename Fn>
constexpr void for_each_field(T&& obj, Fn&& fn) {
  template for (constexpr std::meta::info member : member_array<std::remove_cvref_t<T>>()) {
    fn(obj.[:member:]);
  }
}

/// @brief Invoke fn(field_ref, name) for each non-static data member of obj.
/// The name is the member's identifier, backed by static storage.
template <typename T, typename Fn>
constexpr void for_each_named_field(T&& obj, Fn&& fn) {
  template for (constexpr std::meta::info member : member_array<std::remove_cvref_t<T>>()) {
    fn(obj.[:member:],
       std::string_view(std::define_static_string(std::meta::identifier_of(member))));
  }
}

// clang-format on

}  // namespace relx::refl
