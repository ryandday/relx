#include <gtest/gtest.h>
#include <relx/sql_literal.hpp>

// checked_sql / placeholder_count consteval validation

TEST(SqlLiteralTest, CountsDensePlaceholders) {
  static_assert(relx::placeholder_count<"SELECT 1">() == 0);
  static_assert(relx::placeholder_count<"SELECT * FROM t WHERE id = $1">() == 1);
  static_assert(relx::placeholder_count<"SELECT $2 + $1, $3">() == 3);
  static_assert(relx::placeholder_count<"UPDATE t SET a = $1 WHERE a = $1">() == 1);
  SUCCEED();
}

TEST(SqlLiteralTest, IgnoresPlaceholdersInStringsIdentifiersAndComments) {
  static_assert(relx::placeholder_count<"SELECT '$3' , \"$5\" , $1 -- $9\n FROM t">() == 1);
  static_assert(relx::placeholder_count<"SELECT /* $4 */ $1, $2">() == 2);
  static_assert(relx::placeholder_count<"SELECT 'it''s $7' , $1">() == 1);
  SUCCEED();
}

TEST(SqlLiteralTest, CheckedSqlReturnsText) {
  constexpr std::string_view sql = relx::checked_sql<"SELECT * FROM t WHERE id = $1", 1>();
  EXPECT_EQ(sql, "SELECT * FROM t WHERE id = $1");

  constexpr std::string_view no_params = relx::checked_sql<"SELECT 42">();
  EXPECT_EQ(no_params, "SELECT 42");
}
