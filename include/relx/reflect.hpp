#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202603L
#error "relx requires C++26 reflection (P2996). Build with GCC 16.1+ and -std=c++26 -freflection."
#endif

/// @brief Compile-time struct member iteration built on C++26 reflection (P2996).
/// Replaces the previous Boost.PFR machinery.
namespace relx::refl {

// clang-format off

namespace detail {

/// @brief Collect non-static data members of a type, walking base classes first
/// (declaration/layout order). Base traversal matters because define_aggregate-synthesized
/// types (e.g. table_ref) keep their members in a base class.
consteval void collect_members(std::meta::info type, std::vector<std::meta::info>& out) {
  constexpr auto ctx = std::meta::access_context::unchecked();
  for (std::meta::info base : std::meta::bases_of(type, ctx)) {
    collect_members(std::meta::type_of(base), out);
  }
  for (std::meta::info member : std::meta::nonstatic_data_members_of(type, ctx)) {
    out.push_back(member);
  }
}

}  // namespace detail

/// @brief All non-static data members of T (including inherited ones, bases first)
/// as a static array of std::meta::info
template <typename T>
consteval auto member_array() {
  std::vector<std::meta::info> members;
  detail::collect_members(^^T, members);
  return std::define_static_array(members);
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

/// @brief The identifiers of T's fields (including inherited, bases first), backed by
/// static storage
template <typename T>
consteval auto field_names() {
  std::array<std::string_view, field_count<T>()> names{};
  std::size_t i = 0;
  template for (constexpr std::meta::info m : member_array<T>()) {
    names[i++] = std::define_static_string(std::meta::identifier_of(m));
  }
  return names;
}

/// @brief Whether T has a field with the given identifier
template <typename T>
consteval bool has_field_named(std::string_view name) {
  for (std::string_view field : field_names<T>()) {
    if (field == name) {
      return true;
    }
  }
  return false;
}

/// @brief Human-readable name of a type, backed by static storage
template <typename T>
consteval std::string_view type_name() {
  return std::define_static_string(std::meta::display_string_of(^^T));
}

/// @brief The identifier of an enumerator, or empty for values outside the enumeration
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string_view enum_name(E value) {
  template for (constexpr std::meta::info e :
                std::define_static_array(std::meta::enumerators_of(^^E))) {
    if (value == [:e:]) {
      return std::define_static_string(std::meta::identifier_of(e));
    }
  }
  return {};
}

/// @brief Parse an enumerator from its identifier
template <typename E>
  requires std::is_enum_v<E>
constexpr std::optional<E> enum_cast(std::string_view name) {
  template for (constexpr std::meta::info e :
                std::define_static_array(std::meta::enumerators_of(^^E))) {
    if (name == std::meta::identifier_of(e)) {
      return [:e:];
    }
  }
  return std::nullopt;
}

/// @brief All enumerator identifiers as a SQL quoted list: 'a', 'b', 'c'
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_sql_list() {
  std::string out;
  template for (constexpr std::meta::info e :
                std::define_static_array(std::meta::enumerators_of(^^E))) {
    if (!out.empty()) {
      out += ", ";
    }
    out += '\'';
    out += std::meta::identifier_of(e);
    out += '\'';
  }
  return out;
}

// clang-format on

}  // namespace relx::refl
