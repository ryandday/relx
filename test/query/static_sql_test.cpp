#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
  int age;
  std::optional<std::string> bio;
};
inline constexpr auto users = relx::t<Users>;

struct [[=relx::table("posts")]] Posts {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  std::string title;
};
inline constexpr auto posts = relx::t<Posts>;

// The entire builder chain runs at compile time; the statement lives in static storage

TEST(StaticSqlTest, SelectWhere) {
  static_assert(relx::static_sql(relx::query::select(users.id, users.name)
                                     .from(users)
                                     .where(users.id == 42)) ==
                "SELECT users.id, users.name FROM users WHERE (users.id = ?)");
  SUCCEED();
}

TEST(StaticSqlTest, ValueIndependence) {
  // Same SQL regardless of the bound values - including NULL optionals
  constexpr auto sql_engaged = relx::static_sql(
      relx::query::select(users.id).from(users).where(users.bio ==
                                                      relx::query::val(std::optional<std::string>("x"))));
  constexpr auto sql_null = relx::static_sql(
      relx::query::select(users.id).from(users).where(users.bio ==
                                                      relx::query::val(std::optional<std::string>())));
  static_assert(sql_engaged == sql_null);
  SUCCEED();
}

TEST(StaticSqlTest, JoinGroupOrderLimit) {
  static_assert(relx::static_sql(relx::query::select(users.id)
                                     .from(users)
                                     .join(posts, relx::query::on(users.id == posts.user_id))
                                     .where(users.age > 18)
                                     .group_by(users.id)
                                     .order_by(relx::query::desc(users.age))
                                     .limit(10)
                                     .offset(20)) ==
                "SELECT users.id FROM users "
                "JOIN posts ON (users.id = posts.user_id) "
                "WHERE (users.age > ?) "
                "GROUP BY users.id "
                "ORDER BY users.age DESC "
                "LIMIT ? "
                "OFFSET ?");
  SUCCEED();
}

TEST(StaticSqlTest, SelectAllExpansion) {
  static_assert(relx::static_sql(relx::query::select_all(users)) ==
                "SELECT users.id, users.name, users.age, users.bio FROM users");
  SUCCEED();
}

TEST(StaticSqlTest, PostgresPlaceholders) {
  static_assert(relx::static_pg_sql(relx::query::select(users.id)
                                        .from(users)
                                        .where(users.id == 1 && users.age > 2)
                                        .limit(3)) ==
                "SELECT users.id FROM users WHERE ((users.id = $1) AND (users.age > $2)) "
                "LIMIT $3");
  SUCCEED();
}

TEST(StaticSqlTest, RuntimeToSqlUnchanged) {
  auto query = relx::query::select(users.id).from(users).where(users.id == 7);
  EXPECT_EQ(query.to_sql(), "SELECT users.id FROM users WHERE (users.id = ?)");
}

TEST(StaticSqlTest, InsertUpdateDelete) {
  static_assert(relx::static_sql(relx::query::insert_into(users)
                                     .columns(users.id, users.name)
                                     .values(1, "a")) ==
                "INSERT INTO users (id, name) VALUES (?, ?)");
  static_assert(relx::static_sql(relx::query::update(users)
                                     .set(users.name, "b")
                                     .where(users.id == 1)) ==
                "UPDATE users SET name = ? WHERE (users.id = ?)");
  static_assert(relx::static_sql(relx::query::delete_from(users).where(users.id == 1)) ==
                "DELETE FROM users WHERE (users.id = ?)");
  SUCCEED();
}

// Static shape: a query type whose SQL text is fully determined by the type

TEST(StaticShapeTest, CoreBuildersAreStaticShaped) {
  auto q1 = relx::query::select(users.id, users.name)
                .from(users)
                .join(posts, relx::query::on(users.id == posts.user_id))
                .where(users.age > 18 && users.name == "x")
                .order_by(relx::query::desc(users.age))
                .limit(5);
  static_assert(relx::has_static_shape_v<decltype(q1)>);

  auto q2 = relx::query::insert_into(users).columns(users.id, users.name).values(1, "a");
  static_assert(relx::has_static_shape_v<decltype(q2)>);

  auto q3 = relx::query::update(users).set(users.name, "b").where(users.id == 1);
  static_assert(relx::has_static_shape_v<decltype(q3)>);

  auto q4 = relx::query::delete_from(users).where(users.id == 1);
  static_assert(relx::has_static_shape_v<decltype(q4)>);
  SUCCEED();
}

TEST(StaticShapeTest, ValueDependentQueriesAreNot) {
  // Dynamic IN-list: placeholder count depends on the runtime vector
  std::vector<std::string> names = {"a", "b"};
  auto q1 = relx::query::select(users.id).from(users).where(relx::query::in(users.name, names));
  static_assert(!relx::has_static_shape_v<decltype(q1)>);

  // Runtime alias: the alias string is part of the SQL text
  auto q2 = relx::query::select_expr(relx::query::as(users.id, "uid")).from(users);
  static_assert(!relx::has_static_shape_v<decltype(q2)>);
  SUCCEED();
}

TEST(StaticShapeTest, OperatorAndJoinTypeAreInTheType) {
  // Different operators / join types are different types - the invariant that makes
  // type-keyed SQL memoization sound
  static_assert(!std::is_same_v<decltype(users.id == 1), decltype(users.id > 1)>);
  auto inner = relx::query::select(users.id).from(users).join(
      posts, relx::query::on(users.id == posts.user_id));
  auto left = relx::query::select(users.id).from(users).left_join(
      posts, relx::query::on(users.id == posts.user_id));
  static_assert(!std::is_same_v<decltype(inner), decltype(left)>);
  SUCCEED();
}

// clang-format on

}  // namespace
