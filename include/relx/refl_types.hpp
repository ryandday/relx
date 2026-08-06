#pragma once

#include "reflect.hpp"

#include <meta>
#include <optional>
#include <vector>

/// @brief TypeScript-style utility types built on C++26 reflection: synthesize a
/// variant of a struct with fields removed (omit), kept (pick), or wrapped in
/// std::optional (partial). The synthesized types are plain aggregates - they work as
/// DTOs, values_from() sources, set_from() patches, and where_equals() examples.
///
/// ```cpp
/// using CreateEvent = relx::refl::omit<Event, ^^Event::id, ^^Event::owner_id>;
/// using PatchEvent  = relx::refl::partial<CreateEvent>;
/// using EventRef    = relx::refl::pick<Event, ^^Event::id, ^^Event::title>;
/// ```
namespace relx::refl {

// clang-format off

namespace detail {

consteval bool is_optional_type(std::meta::info type) {
  return std::meta::has_template_arguments(type) &&
         std::meta::template_of(type) == ^^std::optional;
}

}  // namespace detail

/// @brief T without the named members: omit<Event, ^^Event::id, ^^Event::owner_id>
template <typename T, std::meta::info... Excluded>
  requires (sizeof...(Excluded) > 0)
struct omit_impl {
  static_assert(((std::meta::parent_of(Excluded) == std::meta::dealias(^^T)) && ...),
                "omit: every excluded member must be a member of T");
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m : member_array<T>()) {
      if (((m == Excluded) || ...)) {
        continue;
      }
      specs.push_back(std::meta::data_member_spec(std::meta::type_of(m),
                                                  {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};

template <typename T, std::meta::info... Excluded>
using omit = typename omit_impl<T, Excluded...>::type;

/// @brief Only the named members of T: pick<Event, ^^Event::id, ^^Event::title>
template <typename T, std::meta::info... Included>
  requires (sizeof...(Included) > 0)
struct pick_impl {
  static_assert(((std::meta::parent_of(Included) == std::meta::dealias(^^T)) && ...),
                "pick: every included member must be a member of T");
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m : member_array<T>()) {
      if (!((m == Included) || ...)) {
        continue;
      }
      specs.push_back(std::meta::data_member_spec(std::meta::type_of(m),
                                                  {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};

template <typename T, std::meta::info... Included>
using pick = typename pick_impl<T, Included...>::type;

/// @brief Every member of T wrapped in std::optional (already-optional members stay
/// as they are - no optional<optional<...>>). The natural patch type for set_from().
template <typename T>
struct partial_impl {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m : member_array<T>()) {
      const std::meta::info member_type = std::meta::type_of(m);
      const std::meta::info wrapped =
          detail::is_optional_type(std::meta::dealias(member_type))
              ? member_type
              : std::meta::substitute(^^std::optional, {member_type});
      specs.push_back(
          std::meta::data_member_spec(wrapped, {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};

template <typename T>
using partial = typename partial_impl<T>::type;

// clang-format on

}  // namespace relx::refl
