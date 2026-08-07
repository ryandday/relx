#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Guards and identifier-quoting fixes across the query DSL: empty IN lists, alias
// quoting, reserved-word columns outside the SELECT list, consteval DDL table names,
// and validated date-part/interval strings.

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// "order" (table) and "desc" (column) are reserved words; "Sel" has mixed case
struct [[=relx::table("order")]] QuotingOrderTable {
  int id;
  int desc;
};

struct [[=relx::table("MixedCaseTable")]] QuotingMixedTable {
  int id;
};

// clang-format on

inline constexpr auto order_tbl = relx::t<QuotingOrderTable>;

TEST(QueryGuards, EmptyInListIsConstantFalseNotSyntaxError) {
  constexpr auto t = order_tbl;

  const std::vector<int> no_values;
  auto query = relx::query::select(t.id).from(t).where(relx::query::in(t.id, no_values));

  // "IN ()" is a PostgreSQL syntax error; the empty set matches nothing
  EXPECT_NE(query.to_sql().find("1 = 0"), std::string::npos) << query.to_sql();
  EXPECT_EQ(query.to_sql().find("IN ()"), std::string::npos) << query.to_sql();
  EXPECT_TRUE(query.bind_params().empty());

  const std::vector<int> some_values = {1, 2};
  auto non_empty = relx::query::select(t.id).from(t).where(relx::query::in(t.id, some_values));
  EXPECT_NE(non_empty.to_sql().find("IN (?, ?)"), std::string::npos) << non_empty.to_sql();
  EXPECT_EQ(non_empty.bind_params().size(), 2);
}

TEST(QueryGuards, ReservedWordColumnQuotedEverywhere) {
  constexpr auto t = order_tbl;

  // The reserved-word table and column must be quoted in the WHERE/ORDER BY paths
  // (SchemaColumnAdapter), not only in the SELECT list
  auto query = relx::query::select(t.desc).from(t).where(t.desc > 5).order_by(t.desc);
  const std::string sql = query.to_sql();
  EXPECT_NE(sql.find("(\"order\".\"desc\" > ?)"), std::string::npos) << sql;
  EXPECT_NE(sql.find("ORDER BY \"order\".\"desc\""), std::string::npos) << sql;
}

TEST(QueryGuards, MixedCaseAliasQuotedSoItSurvivesFolding) {
  constexpr auto t = order_tbl;

  auto query =
      relx::query::select_expr(relx::query::as(relx::query::count_all(), "TotalCount")).from(t);
  // Unquoted, PostgreSQL would fold TotalCount to totalcount and by-name DTO
  // matching would silently miss
  EXPECT_NE(query.to_sql().find("AS \"TotalCount\""), std::string::npos) << query.to_sql();
}

TEST(QueryGuards, ConstevalDdlQuotesTableName) {
  // The consteval builder must quote like the runtime builder: an unquoted
  // MixedCaseTable would be created folded-lowercase while queries target the
  // quoted name - nothing would match
  constexpr auto create_sql = relx::create_table_sql<QuotingMixedTable>().to_sql();
  EXPECT_NE(std::string(create_sql).find("CREATE TABLE \"MixedCaseTable\""), std::string::npos)
      << create_sql;

  constexpr auto drop_sql = relx::drop_table_sql<QuotingMixedTable>().if_exists().to_sql();
  EXPECT_EQ(std::string(drop_sql), "DROP TABLE IF EXISTS \"MixedCaseTable\";");
}

TEST(QueryGuards, DateUnitAndIntervalValidation) {
  constexpr auto t = order_tbl;
  (void)t;

  // Valid forms build; splice-capable text throws at query build time
  EXPECT_NO_THROW(relx::query::interval("1 day"));
  EXPECT_NO_THROW(relx::query::interval("01:30:00"));
  EXPECT_THROW(relx::query::interval("1 day'; DROP TABLE users; --"), std::invalid_argument);

  auto expr = relx::query::current_date();
  EXPECT_NO_THROW(relx::query::extract("dow", expr));
  EXPECT_THROW(relx::query::extract("dow'); DROP", expr), std::invalid_argument);
  EXPECT_NO_THROW(relx::query::date_trunc("month", expr));
  EXPECT_THROW(relx::query::date_trunc("month) --", expr), std::invalid_argument);
}

TEST(QueryGuards, FixedStringLiteralWorks) {
  using namespace relx::literals;
  constexpr auto name = "users"_fs;
  EXPECT_EQ(std::string_view(name), "users");
}

}  // namespace
