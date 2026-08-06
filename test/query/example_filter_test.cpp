#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/query/example_filter.hpp>
#include <relx/refl_types.hpp>
#include <relx/schema.hpp>

// where_equals(table, example): filter-by-example conditions

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

struct [[=relx::table("ef_users")]] EfUsers {
  [[=relx::ann::pk]] int id;
  std::string name;
  bool active;
  std::optional<std::string> bio;
};
constexpr auto users = relx::t<EfUsers>;

}  // namespace

// clang-format on

TEST(ExampleFilterTest, EngagedFieldsBecomeEqualityFilters) {
  relx::refl::partial<EfUsers> example{};
  example.name = "ada";
  example.active = true;

  auto condition = relx::query::where_equals(users, example);
  EXPECT_EQ(condition.to_sql(), "(ef_users.name = ? AND ef_users.active = ?)");
  auto params = condition.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0].value, "ada");
  EXPECT_EQ(params[1].value, "true");
}

TEST(ExampleFilterTest, EmptyExampleMatchesEverything) {
  const relx::refl::partial<EfUsers> example{};
  auto condition = relx::query::where_equals(users, example);
  EXPECT_EQ(condition.to_sql(), "TRUE");
  EXPECT_TRUE(condition.bind_params().empty());
}

TEST(ExampleFilterTest, ComposesIntoSelectQueries) {
  relx::refl::partial<EfUsers> example{};
  example.active = false;

  auto query = relx::query::select(users.id, users.name)
                   .from(users)
                   .where(relx::query::where_equals(users, example));
  EXPECT_EQ(query.to_sql(),
            "SELECT ef_users.id, ef_users.name FROM ef_users WHERE (ef_users.active = ?)");
  EXPECT_EQ(query.bind_params().size(), 1);
}

TEST(ExampleFilterTest, HandwrittenExampleStructWorks) {
  struct NameOnly {
    std::optional<std::string> name;
  };
  auto condition = relx::query::where_equals(users, NameOnly{.name = "bob"});
  EXPECT_EQ(condition.to_sql(), "(ef_users.name = ?)");
}
