#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

enum class Status { active, pending, suspended };

struct [[=relx::table("accounts")]] Account {
  [[=relx::ann::pk]] int id;
  Status status;
  [[=relx::default_value<Status::pending>{}]] Status review_state;
  std::optional<Status> previous_status;
};
inline constexpr auto accounts = relx::t<Account>;

TEST(EnumColumnTest, EnumUtilities) {
  static_assert(relx::refl::enum_name(Status::pending) == "pending");
  static_assert(relx::refl::enum_cast<Status>("suspended") == Status::suspended);
  static_assert(!relx::refl::enum_cast<Status>("bogus").has_value());
}

TEST(EnumColumnTest, EnumColumnSqlDefinition) {
  EXPECT_EQ(accounts.status.sql_definition(),
            "status TEXT NOT NULL CHECK(status IN ('active', 'pending', 'suspended'))");
}

TEST(EnumColumnTest, EnumDefaultValue) {
  EXPECT_EQ(accounts.review_state.sql_definition(),
            "review_state TEXT NOT NULL DEFAULT 'pending' CHECK(review_state IN ('active', "
            "'pending', 'suspended'))");
}

TEST(EnumColumnTest, OptionalEnumIsNullableWithCheck) {
  EXPECT_EQ(accounts.previous_status.sql_definition(),
            "previous_status TEXT CHECK(previous_status IN ('active', 'pending', 'suspended'))");
}

TEST(EnumColumnTest, EnumWhereConditionBindsIdentifier) {
  auto query = relx::query::select(accounts.id).from(accounts).where(
      accounts.status == Status::active);
  EXPECT_EQ(query.to_sql(), "SELECT accounts.id FROM accounts WHERE (accounts.status = ?)");
  ASSERT_EQ(query.bind_params().size(), 1);
  EXPECT_EQ(query.bind_params()[0], "active");
}

TEST(EnumColumnTest, EnumTraitsRoundTrip) {
  using Traits = relx::schema::column_traits<Status>;
  EXPECT_EQ(Traits::to_sql_string(Status::suspended), "suspended");
  EXPECT_EQ(Traits::from_sql_string("active"), Status::active);
  EXPECT_EQ(Traits::from_sql_string("'pending'"), Status::pending);
  EXPECT_THROW(Traits::from_sql_string("bogus"), std::invalid_argument);
}

// clang-format on

}  // namespace
