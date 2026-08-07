#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Positive half of the type-safety contract: valid combinations compile AND produce
// the expected SQL and parameters. The negative half (invalid combinations must NOT
// compile) lives in the compile-fail suite - type_mismatch_comparison.cpp,
// arithmetic_on_string.cpp, arithmetic_on_bool.cpp, select_zero_columns.cpp - because
// the operators enforce compatibility with hard static_asserts, which only a
// compile-fail harness can pin.

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("test_table")]] type_safety_table {
  int id;
  double price;
  std::string name;
  bool is_active;

  std::optional<int> optional_id;
  std::optional<std::string> optional_name;
  std::optional<double> optional_price;
};

struct [[=relx::table("compatible_table")]] type_safety_compat_table {
  int id;
  std::string name;
};

// clang-format on

inline constexpr auto test_tbl = relx::t<type_safety_table>;
inline constexpr auto compatible_tbl = relx::t<type_safety_compat_table>;

TEST(TypeSafetyTest, ValidComparisonsProduceExpectedSql) {
  constexpr auto t = test_tbl;

  auto by_id = relx::query::select(t.id).from(t).where(t.id == 42);
  EXPECT_EQ(by_id.to_sql(), "SELECT test_table.id FROM test_table WHERE (test_table.id = ?)");
  auto id_params = by_id.bind_params();
  ASSERT_EQ(id_params.size(), 1);
  EXPECT_EQ(id_params[0], "42");

  auto by_price = relx::query::select(t.price).from(t).where(t.price > 10.5);
  EXPECT_EQ(by_price.to_sql(),
            "SELECT test_table.price FROM test_table WHERE (test_table.price > ?)");
  auto price_params = by_price.bind_params();
  ASSERT_EQ(price_params.size(), 1);
  EXPECT_EQ(price_params[0], "10.5");

  auto by_bool = relx::query::select(t.is_active).from(t).where(t.is_active == true);
  auto bool_params = by_bool.bind_params();
  ASSERT_EQ(bool_params.size(), 1);
  EXPECT_EQ(bool_params[0].kind, relx::sql_kind::boolean);
}

TEST(TypeSafetyTest, StringLikeTypesAllComparable) {
  constexpr auto t = test_tbl;

  const std::string as_string = "test";
  const std::string_view as_view = "test";

  auto q1 = relx::query::select(t.name).from(t).where(t.name == "test");
  auto q2 = relx::query::select(t.name).from(t).where(t.name == as_string);
  auto q3 = relx::query::select(t.name).from(t).where(t.name == as_view);

  const std::string expected = "SELECT test_table.name FROM test_table WHERE (test_table.name = ?)";
  EXPECT_EQ(q1.to_sql(), expected);
  EXPECT_EQ(q2.to_sql(), expected);
  EXPECT_EQ(q3.to_sql(), expected);
  EXPECT_EQ(q1.bind_params().at(0), "test");
  EXPECT_EQ(q2.bind_params().at(0), "test");
  EXPECT_EQ(q3.bind_params().at(0), "test");
}

TEST(TypeSafetyTest, OptionalColumnsCompareWithUnderlyingAndOptional) {
  constexpr auto t = test_tbl;

  auto with_value = relx::query::select(t.optional_id).from(t).where(t.optional_id == 42);
  EXPECT_EQ(with_value.to_sql(),
            "SELECT test_table.optional_id FROM test_table WHERE (test_table.optional_id = ?)");
  EXPECT_EQ(with_value.bind_params().at(0), "42");

  const std::optional<int> engaged = 7;
  auto with_optional = relx::query::select(t.optional_id).from(t).where(t.optional_id == engaged);
  EXPECT_EQ(with_optional.bind_params().at(0), "7");

  const std::optional<std::string> opt_name = "alice";
  auto opt_string = relx::query::select(t.optional_name).from(t).where(t.optional_name == opt_name);
  EXPECT_EQ(opt_string.bind_params().at(0), "alice");

  // Non-optional column against an optional value
  auto reversed = relx::query::select(t.id).from(t).where(t.id == engaged);
  EXPECT_EQ(reversed.bind_params().at(0), "7");
}

TEST(TypeSafetyTest, AggregatesOverTypedColumns) {
  constexpr auto t = test_tbl;

  auto summed = relx::query::select_expr(relx::query::sum(t.id)).from(t);
  EXPECT_EQ(summed.to_sql(), "SELECT SUM(test_table.id) FROM test_table");

  auto averaged = relx::query::select_expr(relx::query::avg(t.price)).from(t);
  EXPECT_EQ(averaged.to_sql(), "SELECT AVG(test_table.price) FROM test_table");

  auto min_max = relx::query::select_expr(relx::query::min(t.id), relx::query::max(t.name)).from(t);
  EXPECT_EQ(min_max.to_sql(), "SELECT MIN(test_table.id), MAX(test_table.name) FROM test_table");

  // COUNT works on any column type
  auto counted = relx::query::select_expr(relx::query::count(t.is_active)).from(t);
  EXPECT_EQ(counted.to_sql(), "SELECT COUNT(test_table.is_active) FROM test_table");
}

TEST(TypeSafetyTest, CaseExpressionWithConsistentTypes) {
  constexpr auto t = test_tbl;

  auto valid_case = relx::query::case_()
                        .when(t.id < 10, "Small")
                        .when(t.id < 100, "Medium")
                        .else_("Large")
                        .build();

  auto query = relx::query::select_expr(t.id,
                                        relx::query::as(std::move(valid_case), "size_category"))
                   .from(t);
  EXPECT_NE(query.to_sql().find("CASE WHEN"), std::string::npos) << query.to_sql();
  EXPECT_NE(query.to_sql().find("AS size_category"), std::string::npos) << query.to_sql();
}

TEST(TypeSafetyTest, ColumnToColumnJoinComparison) {
  constexpr auto t1 = test_tbl;
  constexpr auto t2 = compatible_tbl;

  auto valid_join =
      relx::query::select(t1.id, t1.name).from(t1).join(t2, relx::query::on(t1.id == t2.id));
  EXPECT_EQ(valid_join.to_sql(),
            "SELECT test_table.id, test_table.name FROM test_table JOIN compatible_table ON "
            "(test_table.id = compatible_table.id)");
}

TEST(TypeSafetyTest, ArithmeticOnNumericColumns) {
  constexpr auto t = test_tbl;

  auto addition = relx::query::select_expr(t.id + t.optional_id).from(t);
  EXPECT_EQ(addition.to_sql(), "SELECT (test_table.id + test_table.optional_id) FROM test_table");

  auto multiply = relx::query::select_expr(t.price * 1.2).from(t);
  EXPECT_EQ(multiply.to_sql(), "SELECT (test_table.price * ?) FROM test_table");
  EXPECT_EQ(multiply.bind_params().at(0), "1.2");

  auto divide = relx::query::select_expr(t.id / 2).from(t);
  EXPECT_EQ(divide.to_sql(), "SELECT (test_table.id / ?) FROM test_table");
}

TEST(TypeSafetyTest, UpdateAssignmentsWithMatchingTypes) {
  constexpr auto t = test_tbl;

  auto update_query =
      relx::query::update(t).set(t.name, "Updated Name").set(t.price, 99.99).where(t.id == 1);
  const std::string sql = update_query.to_sql();
  EXPECT_NE(sql.find("SET"), std::string::npos) << sql;
  auto params = update_query.bind_params();
  ASSERT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "Updated Name");
  EXPECT_EQ(params[1], "99.99");
  EXPECT_EQ(params[2], "1");
}

TEST(TypeSafetyTest, OrderByTypedColumns) {
  constexpr auto t = test_tbl;

  auto ordered = relx::query::select(t.id, t.name).from(t).order_by(t.name);
  EXPECT_EQ(ordered.to_sql(),
            "SELECT test_table.id, test_table.name FROM test_table ORDER BY test_table.name ASC");
}

}  // namespace
