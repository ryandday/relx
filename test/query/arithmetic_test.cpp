#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct arithmetic_test_table {
  static constexpr auto table_name = "test_table";

  relx::schema::column<arithmetic_test_table, "id", int> id;
  relx::schema::column<arithmetic_test_table, "price", double> price;
  relx::schema::column<arithmetic_test_table, "quantity", int> quantity;
  relx::schema::column<arithmetic_test_table, "discount", double> discount;
  relx::schema::column<arithmetic_test_table, "name", std::string> name;
  relx::schema::column<arithmetic_test_table, "is_active", bool> is_active;

  // Optional columns for testing
  relx::schema::column<arithmetic_test_table, "optional_id", std::optional<int>> optional_id;
  relx::schema::column<arithmetic_test_table, "optional_price", std::optional<double>>
      optional_price;
  relx::schema::column<arithmetic_test_table, "optional_quantity", std::optional<int>>
      optional_quantity;
};

class ArithmeticTest : public ::testing::Test {
protected:
  arithmetic_test_table table;
};

TEST_F(ArithmeticTest, BasicColumnAddition) {
  auto query = relx::query::select_expr(table.id + table.quantity).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.id + test_table.quantity) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, BasicColumnSubtraction) {
  auto query = relx::query::select_expr(table.price - table.discount).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price - test_table.discount) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, BasicColumnMultiplication) {
  auto query = relx::query::select_expr(table.price * table.quantity).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price * test_table.quantity) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, BasicColumnDivision) {
  auto query = relx::query::select_expr(table.price / table.quantity).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price / test_table.quantity) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, ColumnWithValueAddition) {
  auto query = relx::query::select_expr(table.price + 10.5).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price + ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "10.500000");
}

TEST_F(ArithmeticTest, ValueWithColumnAddition) {
  auto query = relx::query::select_expr(5 + table.id).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.id + ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "5");
}

TEST_F(ArithmeticTest, ColumnWithValueSubtraction) {
  auto query = relx::query::select_expr(table.price - 5.0).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price - ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "5.000000");
}

TEST_F(ArithmeticTest, ValueWithColumnSubtraction) {
  auto query = relx::query::select_expr(100 - table.id).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (? - test_table.id) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "100");
}

TEST_F(ArithmeticTest, ColumnWithValueMultiplication) {
  auto query = relx::query::select_expr(table.price * 1.2).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price * ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "1.200000");
}

TEST_F(ArithmeticTest, ValueWithColumnMultiplication) {
  auto query = relx::query::select_expr(2.5 * table.price).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price * ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "2.500000");
}

TEST_F(ArithmeticTest, ColumnWithValueDivision) {
  auto query = relx::query::select_expr(table.price / 2.0).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price / ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "2.000000");
}

TEST_F(ArithmeticTest, ValueWithColumnDivision) {
  auto query = relx::query::select_expr(100.0 / table.price).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (? / test_table.price) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "100.000000");
}

TEST_F(ArithmeticTest, OptionalColumnAddition) {
  auto query = relx::query::select_expr(table.id + table.optional_id).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.id + test_table.optional_id) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, OptionalColumnWithValue) {
  auto query = relx::query::select_expr(table.optional_price * 1.5).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.optional_price * ?) FROM test_table");
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "1.500000");
}

TEST_F(ArithmeticTest, ArithmeticWithAlias) {
  auto total = table.price * table.quantity;
  auto query =
      relx::query::select_expr(table.id, relx::query::as(total, "total_price")).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT test_table.id, (test_table.price * test_table.quantity) AS total_price "
                 "FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, ArithmeticInOrderBy) {
  auto query =
      relx::query::select(table.id, table.price).from(table).order_by(table.price * table.quantity);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT test_table.id, test_table.price FROM test_table ORDER BY "
                 "(test_table.price * test_table.quantity)");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, ArithmeticWithMixedTypes) {
  // int + double should work
  auto mixed = table.id + table.price;
  auto query = relx::query::select_expr(mixed).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.id + test_table.price) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, MultipleSimpleArithmeticColumns) {
  auto query =
      relx::query::select_expr(table.price + 10.0, table.quantity * 2, table.id - 1).from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price + ?), (test_table.quantity * ?), (test_table.id - ?) "
                 "FROM test_table");
  ASSERT_EQ(params.size(), 3);
  EXPECT_EQ(params[0], "10.000000");
  EXPECT_EQ(params[1], "2");
  EXPECT_EQ(params[2], "1");
}

TEST_F(ArithmeticTest, ArithmeticWithOptionalAndRegularColumns) {
  auto query = relx::query::select_expr(table.optional_price + table.price,
                                        table.id * table.optional_quantity)
                   .from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.optional_price + test_table.price), (test_table.id * "
                 "test_table.optional_quantity) FROM test_table");
  EXPECT_TRUE(params.empty());
}

TEST_F(ArithmeticTest, ArithmeticWithIntegerLiterals) {
  auto query = relx::query::select_expr(table.id + 42, table.quantity - 10, table.id * 3,
                                        table.quantity / 2)
                   .from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.id + ?), (test_table.quantity - ?), (test_table.id * ?), "
                 "(test_table.quantity / ?) FROM test_table");
  ASSERT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "42");
  EXPECT_EQ(params[1], "10");
  EXPECT_EQ(params[2], "3");
  EXPECT_EQ(params[3], "2");
}

TEST_F(ArithmeticTest, ArithmeticWithFloatLiterals) {
  auto query = relx::query::select_expr(table.price + 99.99, table.discount - 5.5,
                                        table.price * 0.8, table.discount / 3.14)
                   .from(table);

  auto sql = query.to_sql();
  auto params = query.bind_params();

  EXPECT_EQ(sql, "SELECT (test_table.price + ?), (test_table.discount - ?), (test_table.price * "
                 "?), (test_table.discount / ?) FROM test_table");
  ASSERT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "99.990000");
  EXPECT_EQ(params[1], "5.500000");
  EXPECT_EQ(params[2], "0.800000");
  EXPECT_EQ(params[3], "3.140000");
}

// These tests verify that invalid arithmetic operations fail at compile time
// Uncomment to test compile-time type checking:

// TEST_F(ArithmeticTest, InvalidArithmeticOperations) {
// These should all cause compile-time errors:

// String arithmetic should fail
// auto invalid1 = table.name + table.name;

// Boolean arithmetic should fail
// auto invalid2 = table.is_active * 2;

// String with numeric should fail
// auto invalid3 = table.name + table.id;

// Boolean with numeric should fail
// auto invalid4 = table.is_active / 2.0;

// EXPECT_TRUE(true); // This test is just for documentation
// }
