#include <optional>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <relx/refl_types.hpp>

// pick / omit / partial utility types

namespace {

struct Event {
  int id;
  int owner_id;
  std::string title;
  std::optional<std::string> location;
  bool is_all_day;
};

}  // namespace

TEST(ReflTypesTest, OmitDropsNamedFields) {
  using CreateEvent = relx::refl::omit<Event, ^^Event::id, ^^Event::owner_id>;
  static_assert(std::is_aggregate_v<CreateEvent>);
  static_assert(relx::refl::field_count<CreateEvent>() == 3);
  static_assert(relx::refl::has_field_named<CreateEvent>("title"));
  static_assert(relx::refl::has_field_named<CreateEvent>("location"));
  static_assert(relx::refl::has_field_named<CreateEvent>("is_all_day"));
  static_assert(!relx::refl::has_field_named<CreateEvent>("id"));
  static_assert(!relx::refl::has_field_named<CreateEvent>("owner_id"));

  const CreateEvent event{.title = "standup", .location = "room 4", .is_all_day = false};
  EXPECT_EQ(event.title, "standup");
}

TEST(ReflTypesTest, PickKeepsOnlyNamedFields) {
  using EventRef = relx::refl::pick<Event, ^^Event::id, ^^Event::title>;
  static_assert(relx::refl::field_count<EventRef>() == 2);
  static_assert(relx::refl::has_field_named<EventRef>("id"));
  static_assert(relx::refl::has_field_named<EventRef>("title"));
  static_assert(!relx::refl::has_field_named<EventRef>("owner_id"));

  const EventRef ref{.id = 7, .title = "standup"};
  EXPECT_EQ(ref.id, 7);
}

TEST(ReflTypesTest, PartialWrapsEveryFieldInOptional) {
  using PatchEvent = relx::refl::partial<Event>;
  static_assert(std::is_same_v<decltype(PatchEvent{}.id), std::optional<int>>);
  static_assert(std::is_same_v<decltype(PatchEvent{}.title), std::optional<std::string>>);
  // Already-optional fields stay single-level: no optional<optional<...>>
  static_assert(std::is_same_v<decltype(PatchEvent{}.location), std::optional<std::string>>);

  const PatchEvent patch{};
  EXPECT_FALSE(patch.id.has_value());
}

TEST(ReflTypesTest, UtilityTypesCompose) {
  using CreateEvent = relx::refl::omit<Event, ^^Event::id, ^^Event::owner_id>;
  using PatchCreate = relx::refl::partial<CreateEvent>;
  static_assert(relx::refl::field_count<PatchCreate>() == 3);
  static_assert(std::is_same_v<decltype(PatchCreate{}.title), std::optional<std::string>>);
  static_assert(std::is_same_v<decltype(PatchCreate{}.is_all_day), std::optional<bool>>);
  SUCCEED();
}

TEST(ReflTypesTest, FieldOrderIsPreserved) {
  using CreateEvent = relx::refl::omit<Event, ^^Event::owner_id>;
  constexpr auto names = relx::refl::field_names<CreateEvent>();
  ASSERT_EQ(names.size(), 4);
  EXPECT_EQ(names[0], "id");
  EXPECT_EQ(names[1], "title");
  EXPECT_EQ(names[2], "location");
  EXPECT_EQ(names[3], "is_all_day");
}
