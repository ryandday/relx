#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
  int age;
};
inline constexpr auto users = relx::t<Users>;

struct [[=relx::table("orders")]] Orders {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  double total;
};
inline constexpr auto orders = relx::t<Orders>;

// clang-format on

TEST(TypedInTest, IntValuesBindTyped) {
  auto query =
      relx::select(users.name).from(users).where(relx::in(users.id, std::vector<int>{1, 2, 3}));

  EXPECT_EQ(query.to_sql(), "SELECT users.name FROM users WHERE users.id IN (?, ?, ?)");

  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "1");
  EXPECT_EQ(params[1], "2");
  EXPECT_EQ(params[2], "3");
  // Values bind typed (int4), not as untyped text
  EXPECT_EQ(params[0].kind, relx::sql_kind::int4);
}

TEST(TypedInTest, DoubleValuesBindTyped) {
  auto query = relx::select(orders.id).from(orders).where(
      relx::in(orders.total, std::vector<double>{9.99, 19.99}));

  EXPECT_EQ(query.to_sql(), "SELECT orders.id FROM orders WHERE orders.total IN (?, ?)");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0].kind, relx::sql_kind::float8);
}

TEST(TypedInTest, StringValuesStillWork) {
  auto query = relx::select(users.id).from(users).where(
      relx::in(users.name, std::vector<std::string>{"alice", "bob"}));

  EXPECT_EQ(query.to_sql(), "SELECT users.id FROM users WHERE users.name IN (?, ?)");
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "alice");
  EXPECT_EQ(params[1], "bob");
}

TEST(SubqueryTest, InSubquery) {
  auto sub = relx::select(orders.user_id).from(orders).where(orders.total > 100.0);
  auto query = relx::select(users.name).from(users).where(relx::in(users.id, sub));

  EXPECT_EQ(query.to_sql(), "SELECT users.name FROM users WHERE users.id IN "
                            "(SELECT orders.user_id FROM orders WHERE (orders.total > ?))");

  // The subquery's bind params travel with the outer query
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "100");
  EXPECT_EQ(params[0].kind, relx::sql_kind::float8);
}

TEST(SubqueryTest, Exists) {
  auto sub = relx::select(orders.id).from(orders).where(orders.total > 50.0);
  auto query = relx::select(users.name).from(users).where(relx::exists(sub));

  EXPECT_EQ(query.to_sql(), "SELECT users.name FROM users WHERE EXISTS "
                            "(SELECT orders.id FROM orders WHERE (orders.total > ?))");
  ASSERT_EQ(query.bind_params().size(), 1);
}

TEST(SubqueryTest, NotExists) {
  auto sub = relx::select(orders.id).from(orders);
  auto query = relx::select(users.name).from(users).where(relx::not_exists(sub));

  EXPECT_EQ(query.to_sql(),
            "SELECT users.name FROM users WHERE NOT EXISTS (SELECT orders.id FROM orders)");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SubqueryTest, CorrelatedExists) {
  // Correlation: the subquery references the outer table's column
  auto sub = relx::select(orders.id).from(orders).where(orders.user_id == users.id &&
                                                        orders.total > 50.0);
  auto query = relx::select(users.name).from(users).where(relx::exists(sub));

  EXPECT_EQ(query.to_sql(), "SELECT users.name FROM users WHERE EXISTS "
                            "(SELECT orders.id FROM orders WHERE "
                            "((orders.user_id = users.id) AND (orders.total > ?)))");
}

TEST(SubqueryTest, SubqueryParamOrderFollowsOuterParams) {
  auto sub = relx::select(orders.user_id).from(orders).where(orders.total > 100.0);
  auto query =
      relx::select(users.name).from(users).where(users.age > 18 && relx::in(users.id, sub));

  // Outer param (18) first, then the subquery's (100)
  auto params = query.bind_params();
  ASSERT_EQ(params.size(), 2);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "100");
}

}  // namespace
