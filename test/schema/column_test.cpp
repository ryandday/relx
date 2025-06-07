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
  EXPECT_EQ(price_col.to_sql_string(42.5), "42.500000");
  EXPECT_EQ(price_col.to_sql_string(-123.45), "-123.450000");
  EXPECT_EQ(price_col.to_sql_string(0.0), "0.000000");

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

  // Test converting SQL string to string (with unescaping)
  EXPECT_EQ(name_col.from_sql_string("'hello'"), "hello");
  EXPECT_EQ(name_col.from_sql_string("'O''Reilly'"), "O'Reilly");  // Single quote unescaping
  EXPECT_EQ(name_col.from_sql_string("''"), "");

  // Test with string not wrapped in quotes
  EXPECT_EQ(name_col.from_sql_string("hello"), "hello");
}

TEST(ColumnTest, BooleanConversion) {
  column<DummyTable, "active", bool> active_col;

  // Test converting bool to SQL string
  EXPECT_EQ(active_col.to_sql_string(true), "1");
  EXPECT_EQ(active_col.to_sql_string(false), "0");

  // Test converting SQL string to bool
  EXPECT_TRUE(active_col.from_sql_string("1"));
  EXPECT_TRUE(active_col.from_sql_string("true"));
  EXPECT_TRUE(active_col.from_sql_string("TRUE"));
  EXPECT_FALSE(active_col.from_sql_string("0"));
  EXPECT_FALSE(active_col.from_sql_string("false"));
  EXPECT_FALSE(active_col.from_sql_string("FALSE"));
  EXPECT_FALSE(active_col.from_sql_string("other"));
}

TEST(ColumnTest, ColumnWithLongName) {
  column<DummyTable, "very_long_column_name_that_tests_the_fixed_string_implementation", int>
      long_name_col;

  EXPECT_EQ(std::string_view(long_name_col.name),
            "very_long_column_name_that_tests_the_fixed_string_implementation");
  EXPECT_EQ(long_name_col.sql_definition(),
            "very_long_column_name_that_tests_the_fixed_string_implementation INTEGER NOT NULL");
}