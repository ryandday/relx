#include <string>

#include <gtest/gtest.h>
#include <relx/schema/column.hpp>

using namespace relx::schema;

struct DummyTable {};

TEST(ColumnTest, BasicProperties) {
  // Test regular column
  column<DummyTable, "id", int> id_col;
  EXPECT_EQ(std::string_view(id_col.name), "id");
  EXPECT_EQ(std::string_view(id_col.sql_type), "INTEGER");
  EXPECT_FALSE(id_col.nullable);

  // Test SQL definition
  EXPECT_EQ(id_col.sql_definition(), "id INTEGER NOT NULL");

  // Test nullable column
  column<DummyTable, "name", std::optional<std::string>> name_col;
  EXPECT_EQ(std::string_view(name_col.name), "name");
  EXPECT_EQ(std::string_view(name_col.sql_type), "TEXT");
  EXPECT_TRUE(name_col.nullable);

  // Test SQL definition of nullable column
  EXPECT_EQ(name_col.sql_definition(), "name TEXT");
}

TEST(ColumnTest, IntegerConversion) {
  column<DummyTable, "id", int> id_col;

  // Test converting int to SQL string
  EXPECT_EQ(id_col.to_sql_string(42), "42");
  EXPECT_EQ(id_col.to_sql_string(-123), "-123");
  EXPECT_EQ(id_col.to_sql_string(0), "0");

  // Test converting SQL string to int
  EXPECT_EQ(id_col.from_sql_string("42"), 42);
  EXPECT_EQ(id_col.from_sql_string("-123"), -123);
  EXPECT_EQ(id_col.from_sql_string("0"), 0);
}

TEST(ColumnTest, DoubleConversion) {
  column<DummyTable, "price", double> price_col;

  // Test converting double to SQL string
  EXPECT_EQ(price_col.to_sql_string(42.5), "42.5");
  EXPECT_EQ(price_col.to_sql_string(-123.45), "-123.45");
  EXPECT_EQ(price_col.to_sql_string(0.0), "0");

  // Test converting SQL string to double
  EXPECT_DOUBLE_EQ(price_col.from_sql_string("42.5"), 42.5);
  EXPECT_DOUBLE_EQ(price_col.from_sql_string("-123.45"), -123.45);
  EXPECT_DOUBLE_EQ(price_col.from_sql_string("0.0"), 0.0);
}

TEST(ColumnTest, StringConversion) {
  column<DummyTable, "name", std::string> name_col;

  // Test converting string to SQL string (with escaping)
  EXPECT_EQ(name_col.to_sql_string("hello"), "'hello'");
  EXPECT_EQ(name_col.to_sql_string("O'Reilly"), "'O''Reilly'");  // Single quote escaping
  EXPECT_EQ(name_col.to_sql_string(""), "''");

  // from_sql_string parses raw protocol text verbatim - no SQL-literal de-quoting,
  // so data that happens to be wrapped in quotes survives untouched
  EXPECT_EQ(name_col.from_sql_string("hello"), "hello");
  EXPECT_EQ(name_col.from_sql_string("'hello'"), "'hello'");
  EXPECT_EQ(name_col.from_sql_string(""), "");
}

TEST(ColumnTest, BooleanConversion) {
  column<DummyTable, "active", bool> active_col;

  // Test converting bool to SQL string
  EXPECT_EQ(active_col.to_sql_string(true), "true");
  EXPECT_EQ(active_col.to_sql_string(false), "false");

  // Test converting SQL string to bool
  EXPECT_TRUE(active_col.from_sql_string("1"));
  EXPECT_TRUE(active_col.from_sql_string("true"));
  EXPECT_TRUE(active_col.from_sql_string("TRUE"));
  EXPECT_FALSE(active_col.from_sql_string("0"));
  EXPECT_FALSE(active_col.from_sql_string("false"));
  EXPECT_FALSE(active_col.from_sql_string("FALSE"));

  // Unrecognized text is an error, not false
  EXPECT_THROW(active_col.from_sql_string("other"), std::invalid_argument);
}

TEST(ColumnTest, ColumnWithLongName) {
  column<DummyTable, "very_long_column_name_that_tests_the_fixed_string_implementation", int>
      long_name_col;

  EXPECT_EQ(std::string_view(long_name_col.name),
            "very_long_column_name_that_tests_the_fixed_string_implementation");
  EXPECT_EQ(long_name_col.sql_definition(),
            "very_long_column_name_that_tests_the_fixed_string_implementation INTEGER NOT NULL");
}
TEST(ColumnTest, IdentifierQuoting) {
  using relx::schema::quote_identifier;

  // Safe lowercase identifiers pass through bare
  EXPECT_EQ(quote_identifier("users"), "users");
  EXPECT_EQ(quote_identifier("user_id2"), "user_id2");

  // Reserved keywords, mixed case, and special characters are quoted
  EXPECT_EQ(quote_identifier("order"), "\"order\"");
  EXPECT_EQ(quote_identifier("default"), "\"default\"");
  EXPECT_EQ(quote_identifier("createdAt"), "\"createdAt\"");
  EXPECT_EQ(quote_identifier("2fast"), "\"2fast\"");
  EXPECT_EQ(quote_identifier("weird\"name"), "\"weird\"\"name\"");

  // Quoting flows into column definitions
  column<DummyTable, "default", int> reserved_col;
  EXPECT_EQ(reserved_col.sql_definition(), "\"default\" INTEGER NOT NULL");
}
