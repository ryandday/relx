#include <optional>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Table aliases: relx::t<T, "alias"> - distinct types per alias, aliased FROM/JOIN
// rendering, self-joins

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

struct [[=relx::table("ta_employees")]] Employee {
  [[=relx::ann::pk]] int id;
  std::string name;
  int manager_id;
};
constexpr auto emp = relx::t<Employee>;
constexpr auto mgr = relx::t<Employee, "m">;

}  // namespace

// clang-format on

TEST(TableAliasTest, AliasedTableIsADistinctType) {
  static_assert(!std::is_same_v<decltype(emp), decltype(mgr)>);
  static_assert(!std::is_same_v<decltype(emp.id), decltype(mgr.id)>);
  SUCCEED();
}

TEST(TableAliasTest, AliasedColumnsQualifyWithTheAlias) {
  auto query = relx::query::select(mgr.id, mgr.name).from(mgr);
  EXPECT_EQ(query.to_sql(), "SELECT m.id, m.name FROM ta_employees AS m");
}

TEST(TableAliasTest, SelfJoinRendersBothSides) {
  auto query = relx::query::select(emp.name, relx::as<"manager_name">(mgr.name))
                   .from(emp)
                   .join(mgr, relx::query::on(emp.manager_id == mgr.id));
  EXPECT_EQ(query.to_sql(), "SELECT ta_employees.name, m.name AS manager_name FROM ta_employees "
                            "JOIN ta_employees AS m ON (ta_employees.manager_id = m.id)");
}

TEST(TableAliasTest, TwoAliasesOfOneTableInOneQuery) {
  constexpr auto a = relx::t<Employee, "a">;
  constexpr auto b = relx::t<Employee, "b">;
  auto query =
      relx::query::select(a.id, b.id).from(a).join(b, relx::query::on(a.manager_id == b.id));
  EXPECT_EQ(query.to_sql(), "SELECT a.id, b.id FROM ta_employees AS a "
                            "JOIN ta_employees AS b ON (a.manager_id = b.id)");
}

TEST(TableAliasTest, WholeTableSelectNestsUnderTheAlias) {
  auto query = relx::query::select(emp, mgr).from(emp).left_join(
      mgr, relx::query::on(emp.manager_id == mgr.id));
  using Row = relx::row_type_for<decltype(query)>;

  static_assert(std::is_same_v<decltype(Row{}.ta_employees), Employee>);
  static_assert(std::is_same_v<decltype(Row{}.m), std::optional<Employee>>);
  SUCCEED();
}

TEST(TableAliasTest, StaticSqlWorksWithAliases) {
  static_assert(relx::static_sql(relx::query::select(mgr.id).from(mgr).where(mgr.id == 1)) ==
                "SELECT m.id FROM ta_employees AS m WHERE (m.id = ?)");
  SUCCEED();
}

TEST(TableAliasTest, SelectAllOverAliasedTable) {
  auto query = relx::query::select_all(mgr);
  EXPECT_EQ(query.to_sql(), "SELECT m.id, m.name, m.manager_id FROM ta_employees AS m");
}
