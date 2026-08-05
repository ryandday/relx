// Prototype: TypeScript-style utility types via P2996 + glaze round-trip.
// omit<T, ^^T::field...>, pick<T, ^^T::field...>, partial<T>
#include <cstdio>
#include <meta>
#include <optional>
#include <string>
#include <vector>

#include <glaze/glaze.hpp>

namespace refl {

consteval bool is_optional(std::meta::info type) {
  return std::meta::has_template_arguments(type) && std::meta::template_of(type) == ^^std::optional;
}

template <typename T, std::meta::info... Excluded>
struct omit_impl {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m :
         std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
      if (((m == Excluded) || ...)) continue;
      specs.push_back(std::meta::data_member_spec(std::meta::type_of(m),
                                                  {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};
template <typename T, std::meta::info... Ex>
using omit = typename omit_impl<T, Ex...>::type;

template <typename T, std::meta::info... Included>
struct pick_impl {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m :
         std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
      if (!((m == Included) || ...)) continue;
      specs.push_back(std::meta::data_member_spec(std::meta::type_of(m),
                                                  {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};
template <typename T, std::meta::info... In>
using pick = typename pick_impl<T, In...>::type;

template <typename T>
struct partial_impl {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m :
         std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
      std::meta::info t = std::meta::type_of(m);
      std::meta::info wrapped = is_optional(t) ? t
                                               : std::meta::substitute(^^std::optional, {
                                                                                            t});
      specs.push_back(std::meta::data_member_spec(wrapped, {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};
template <typename T>
using partial = typename partial_impl<T>::type;

}  // namespace refl

struct Event {
  int id;
  int owner_id;
  std::string title;
  std::optional<std::string> location;
  bool is_all_day;
};

using CreateEvent = refl::omit<Event, ^^Event::id, ^^Event::owner_id>;
using PatchEvent = refl::partial<CreateEvent>;
using EventRef = refl::pick<Event, ^^Event::id, ^^Event::title>;

// Composition + idempotence checks
static_assert(std::is_aggregate_v<CreateEvent>);
static_assert(std::is_same_v<decltype(PatchEvent{}.title), std::optional<std::string>>);
static_assert(std::is_same_v<decltype(PatchEvent{}.location),
                             std::optional<std::string>>);  // not optional<optional<...>>

int main() {
  CreateEvent c{.title = "standup", .location = "room 4", .is_all_day = false};
  auto out = glz::write_json(c);
  std::printf("create json: %s\n", out ? out->c_str() : "WRITE FAILED");

  auto patch = glz::read_json<PatchEvent>(R"({"title":"retro"})");
  if (!patch) {
    std::printf("patch read FAILED\n");
    return 1;
  }
  std::printf("patch: title=%s location_set=%d\n", patch->title ? patch->title->c_str() : "(unset)",
              (int)patch->location.has_value());

  CreateEvent incomplete{};
  auto missing_ec = glz::read<glz::opts{.error_on_missing_keys = true}>(incomplete,
                                                                        R"({"location":"x"})");
  std::printf("missing required title rejected: %d\n", (int)(bool)missing_ec);

  auto ref = glz::write_json(EventRef{.id = 7, .title = "standup"});
  std::printf("pick json: %s\n", ref ? ref->c_str() : "WRITE FAILED");
  return 0;
}
