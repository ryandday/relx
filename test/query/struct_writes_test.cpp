#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Struct-based writes: insert_into(t).values_from(obj), .upsert(), update(t).set_from(patch)

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

enum class Plan { free, pro };

struct [[=relx::table("sw_users")]] SwUsers {
  [[=relx::ann::pk, =relx::ann::identity]] int id;
  std::string username;
  [[=relx::ann::unique]] std::string email;
  bool active;
  Plan plan;
  std::optional<std::string> bio;
};
constexpr auto sw_users = relx::t<SwUsers>;

// Natural (non-generated) single-column pk
struct [[=relx::table("sw_settings")]] SwSettings {
  [[=relx::ann::pk]] std::string key;
  std::string value;
};
constexpr auto sw_settings = relx::t<SwSettings>;

// Composite pk, no generated columns
struct [[=relx::table("sw_pairs"), =relx::ann::composite_pk("a", "b")]] SwPairs {
  int a;
  int b;
  std::string note;
};
constexpr auto sw_pairs = relx::t<SwPairs>;

// Pure join table: every column is part of the key
struct [[=relx::table("sw_links"), =relx::ann::composite_pk("x", "y")]] SwLinks {
  int x;
  int y;
};
constexpr auto sw_links = relx::t<SwLinks>;

struct UserPatch {
  std::optional<std::string> email;
  std::optional<bool> active;
  std::optional<std::string> bio;
};

}  // namespace

// clang-format on

TEST(StructWritesTest, ValuesFromSkipsIdentityAndExpandsFields) {
  const SwUsers u{.id = 0,
                  .username = "ada",
                  .email = "ada@example.com",
                  .active = true,
                  .plan = Plan::pro,
                  .bio = std::nullopt};
  auto query = relx::insert_into(sw_users).values_from(u);

  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_users (username, email, active, plan, bio) "
                            "VALUES (?, ?, ?, ?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 5);
  EXPECT_EQ(params[0].value, "ada");
  EXPECT_EQ(params[1].value, "ada@example.com");
  EXPECT_EQ(params[2].value, "true");
  EXPECT_EQ(params[3].value, "pro");
  EXPECT_TRUE(params[4].is_null);
}

TEST(StructWritesTest, ValuesFromEngagedOptionalBindsValue) {
  const SwUsers u{.id = 0,
                  .username = "bob",
                  .email = "bob@example.com",
                  .active = false,
                  .plan = Plan::free,
                  .bio = "hello"};
  auto params = relx::insert_into(sw_users).values_from(u).bind_params();
  ASSERT_EQ(params.size(), 5);
  EXPECT_FALSE(params[4].is_null);
  EXPECT_EQ(params[4].value, "hello");
}

TEST(StructWritesTest, ValuesFromMultipleObjectsAddsOneRowEach) {
  const SwUsers u{.id = 0,
                  .username = "ada",
                  .email = "ada@example.com",
                  .active = true,
                  .plan = Plan::pro,
                  .bio = std::nullopt};
  const SwUsers v{.id = 0,
                  .username = "bob",
                  .email = "bob@example.com",
                  .active = false,
                  .plan = Plan::free,
                  .bio = "hi"};
  auto query = relx::insert_into(sw_users).values_from(u, v);

  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_users (username, email, active, plan, bio) "
                            "VALUES (?, ?, ?, ?, ?), (?, ?, ?, ?, ?)");
  EXPECT_EQ(query.bind_params().size(), 10);
}

TEST(StructWritesTest, ValuesFromWithoutGeneratedColumnsInsertsEverything) {
  const SwSettings s{.key = "theme", .value = "dark"};
  auto query = relx::insert_into(sw_settings).values_from(s);
  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_settings (key, value) VALUES (?, ?)");
}

TEST(StructWritesTest, ValuesFromWithReturning) {
  const SwUsers u{.id = 0,
                  .username = "ada",
                  .email = "ada@example.com",
                  .active = true,
                  .plan = Plan::pro,
                  .bio = std::nullopt};
  auto query = relx::insert_into(sw_users).values_from(u).returning(sw_users.id);
  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_users (username, email, active, plan, bio) "
                            "VALUES (?, ?, ?, ?, ?) RETURNING sw_users.id");
}

TEST(StructWritesTest, UpsertOnNaturalPkUpdatesNonKeyColumns) {
  const SwSettings s{.key = "theme", .value = "dark"};
  auto query = relx::insert_into(sw_settings).values_from(s).upsert();
  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_settings (key, value) VALUES (?, ?) "
                            "ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
}

TEST(StructWritesTest, UpsertOnCompositePkUpdatesNonKeyColumns) {
  const SwPairs p{.a = 1, .b = 2, .note = "n"};
  auto query = relx::insert_into(sw_pairs).values_from(p).upsert();
  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_pairs (a, b, note) VALUES (?, ?, ?) "
                            "ON CONFLICT (a, b) DO UPDATE SET note = EXCLUDED.note");
}

TEST(StructWritesTest, UpsertDegradesToDoNothingWhenOnlyKeyColumnsInserted) {
  const SwLinks l{.x = 1, .y = 2};
  auto query = relx::insert_into(sw_links).values_from(l).upsert();
  EXPECT_EQ(query.to_sql(),
            "INSERT INTO sw_links (x, y) VALUES (?, ?) ON CONFLICT (x, y) DO NOTHING");
}

TEST(StructWritesTest, UpsertWithReturningKeepsClauseOrder) {
  const SwSettings s{.key = "theme", .value = "dark"};
  auto query = relx::insert_into(sw_settings).values_from(s).upsert().returning(sw_settings.value);
  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_settings (key, value) VALUES (?, ?) "
                            "ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value "
                            "RETURNING sw_settings.value");
}

TEST(StructWritesTest, UpsertAfterExplicitColumns) {
  auto query = relx::insert_into(sw_settings)
                   .columns(sw_settings.key, sw_settings.value)
                   .values("theme", "dark")
                   .upsert();
  EXPECT_EQ(query.to_sql(), "INSERT INTO sw_settings (key, value) VALUES (?, ?) "
                            "ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
}

TEST(StructWritesTest, SetFromUsesOnlyEngagedFields) {
  const UserPatch patch{.email = "new@example.com", .active = std::nullopt, .bio = std::nullopt};
  auto query = relx::update(sw_users).set_from(patch).where(sw_users.id == 42);

  EXPECT_EQ(query.to_sql(), "UPDATE sw_users SET email = ? WHERE (sw_users.id = ?)");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0].value, "new@example.com");
  EXPECT_EQ(params[1].value, "42");
}

TEST(StructWritesTest, SetFromWithAllFieldsEngaged) {
  const UserPatch patch{.email = "e@example.com", .active = false, .bio = "b"};
  auto query = relx::update(sw_users).set_from(patch).where(sw_users.id == 1);

  EXPECT_EQ(query.to_sql(),
            "UPDATE sw_users SET email = ?, active = ?, bio = ? WHERE (sw_users.id = ?)");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 4);
  EXPECT_EQ(params[0].value, "e@example.com");
  EXPECT_EQ(params[1].value, "false");
  EXPECT_EQ(params[2].value, "b");
}

TEST(StructWritesTest, SetFromEmptyPatchProducesNoSetList) {
  // An all-disengaged patch renders an UPDATE without SET assignments — invalid SQL
  // the server rejects. has_engaged_fields is the documented guard.
  const UserPatch patch{};
  EXPECT_FALSE(relx::has_engaged_fields(patch));

  auto query = relx::update(sw_users).set_from(patch).where(sw_users.id == 1);
  EXPECT_EQ(query.to_sql(), "UPDATE sw_users SET  WHERE (sw_users.id = ?)");
  EXPECT_EQ(query.bind_params().size(), 1);
}

TEST(StructWritesTest, HasEngagedFieldsDetectsAnyEngagedField) {
  EXPECT_TRUE(relx::has_engaged_fields(
      UserPatch{.email = std::nullopt, .active = true, .bio = std::nullopt}));
  EXPECT_FALSE(relx::has_engaged_fields(UserPatch{}));
}

TEST(StructWritesTest, SetFromComposesWithExplicitSet) {
  const UserPatch patch{.email = "e@example.com", .active = std::nullopt, .bio = std::nullopt};
  auto query =
      relx::update(sw_users).set(sw_users.active, false).set_from(patch).where(sw_users.id == 7);
  EXPECT_EQ(query.to_sql(), "UPDATE sw_users SET active = ?, email = ? WHERE (sw_users.id = ?)");
  EXPECT_EQ(query.bind_params().size(), 3);
}

TEST(StructWritesTest, SetFromWithReturning) {
  const UserPatch patch{.email = "e@example.com", .active = std::nullopt, .bio = std::nullopt};
  auto query =
      relx::update(sw_users).set_from(patch).where(sw_users.id == 7).returning(sw_users.email);
  EXPECT_EQ(query.to_sql(),
            "UPDATE sw_users SET email = ? WHERE (sw_users.id = ?) RETURNING sw_users.email");
}
