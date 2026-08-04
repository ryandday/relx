#pragma once

#include <meta>
#include <optional>
#include <string>
#include <string_view>

namespace relx::web {

/// @brief Marks a hand-written DTO as a projection of a table struct:
///
/// ```cpp
/// struct [[=relx::web::projects<Event>]] CreateEvent {
///   std::string title;
///   std::optional<std::string> location;
/// };
/// ```
///
/// Every DTO field must name a member of the table with the same underlying
/// type (the DTO may add optionality, e.g. optional<string> over a string
/// column for PATCH shapes). The check fires wherever the DTO is consumed —
/// read_body<T> — so a stale DTO is a compile error naming the drifted field.
template <typename Table>
struct projects_t {
  using table_type = Table;
};

template <typename Table>
inline constexpr projects_t<Table> projects{};

namespace detail {

consteval std::meta::info unwrap_optional_info(std::meta::info type) {
  type = std::meta::dealias(type);
  if (std::meta::has_template_arguments(type) &&
      std::meta::template_of(type) == ^^std::optional) {
    return std::meta::dealias(std::meta::template_arguments_of(type)[0]);
  }
  return type;
}

/// @brief The table named by a projects<> annotation on Dto, or ^^void
template <typename Dto>
consteval std::meta::info projected_table_of() {
  for (std::meta::info a : std::meta::annotations_of(^^Dto)) {
    std::meta::info type = std::meta::remove_cv(std::meta::type_of(a));
    if (std::meta::has_template_arguments(type) &&
        std::meta::template_of(type) == ^^projects_t) {
      return std::meta::template_arguments_of(type)[0];
    }
  }
  return ^^void;
}

template <typename Dto>
consteval bool has_projection() {
  return projected_table_of<Dto>() != ^^void;
}

/// @brief Per-field mismatches between Dto and Table, empty when the DTO projects cleanly
template <typename Dto, typename Table>
consteval std::string projection_errors() {
  constexpr auto ctx = std::meta::access_context::unchecked();
  std::string diag;
  for (std::meta::info dto_member : std::meta::nonstatic_data_members_of(^^Dto, ctx)) {
    const std::string_view name = std::meta::identifier_of(dto_member);
    bool found = false;
    for (std::meta::info table_member : std::meta::nonstatic_data_members_of(^^Table, ctx)) {
      if (std::meta::identifier_of(table_member) != name) {
        continue;
      }
      found = true;
      if (unwrap_optional_info(std::meta::type_of(dto_member)) !=
          unwrap_optional_info(std::meta::type_of(table_member))) {
        diag += "field '";
        diag += name;
        diag += "' type differs from its column; ";
      }
    }
    if (!found) {
      diag += "field '";
      diag += name;
      diag += "' names no column of the table; ";
    }
  }
  return diag;
}

}  // namespace detail

/// @brief Diagnostic for a DTO carrying a projects<> annotation, empty when valid.
/// Backed by static storage so it can be a static_assert message.
template <typename Dto>
consteval std::string_view projection_diagnostic() {
  if constexpr (detail::has_projection<Dto>()) {
    using Table = [:detail::projected_table_of<Dto>():];
    return std::define_static_string(detail::projection_errors<Dto, Table>());
  } else {
    return {};
  }
}

/// @brief Compile-time enforcement point: no-op for DTOs without a projects<>
/// annotation, a static_assert naming the drifted fields otherwise
template <typename Dto>
constexpr void enforce_projection() {
  if constexpr (detail::has_projection<Dto>()) {
    constexpr std::string_view diag = projection_diagnostic<Dto>();
    static_assert(diag.empty(), diag);
  }
}

}  // namespace relx::web
