#pragma once

#include "reflect.hpp"

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/// @brief Debug pretty-printer for aggregate values (DTOs, synthesized rows, nested
/// join rows): relx::debug::dump(row) renders "Type{field: value, ...}" recursively.
/// Strictly a diagnostic aid - the output format is not stable API.
namespace relx::debug {

namespace detail {

template <typename T>
inline constexpr bool is_optional_v = false;
template <typename U>
inline constexpr bool is_optional_v<std::optional<U>> = true;

template <typename T>
inline constexpr bool is_vector_v = false;
template <typename U>
inline constexpr bool is_vector_v<std::vector<U>> = true;

template <typename T>
void append_value(std::string& out, const T& value) {
  using V = std::remove_cvref_t<T>;
  if constexpr (is_optional_v<V>) {
    if (value.has_value()) {
      append_value(out, *value);
    } else {
      out += "null";
    }
  } else if constexpr (std::is_same_v<V, bool>) {
    out += value ? "true" : "false";
  } else if constexpr (std::is_enum_v<V>) {
    const std::string_view name = refl::enum_name(value);
    if (name.empty()) {
      out += std::format("<enum:{}>", static_cast<long long>(value));
    } else {
      out += name;
    }
  } else if constexpr (std::is_arithmetic_v<V>) {
    out += std::format("{}", value);
  } else if constexpr (std::is_convertible_v<V, std::string_view>) {
    out += '"';
    out += std::string_view(value);
    out += '"';
  } else if constexpr (is_vector_v<V>) {
    out += '[';
    bool first = true;
    for (const auto& element : value) {
      if (!first) {
        out += ", ";
      }
      first = false;
      append_value(out, element);
    }
    out += ']';
  } else if constexpr (std::is_class_v<V> && std::is_aggregate_v<V>) {
    out += refl::type_name<V>();
    out += '{';
    bool first = true;
    refl::for_each_named_field(value, [&](const auto& field, std::string_view name) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += name;
      out += ": ";
      append_value(out, field);
    });
    out += '}';
  } else if constexpr (requires(std::string s) { s = std::format("{}", value); }) {
    out += std::format("{}", value);
  } else {
    out += "<unprintable>";
  }
}

}  // namespace detail

/// @brief Render an aggregate (or any supported value) as human-readable text
template <typename T>
std::string dump(const T& value) {
  std::string out;
  detail::append_value(out, value);
  return out;
}

}  // namespace relx::debug
