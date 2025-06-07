#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct test_table {
  static constexpr auto table_name = "test_table";

  relx::schema::column<test_table, "id", int> id;
  relx::schema::column<test_table, "price", double> price;
  relx::schema::column<test_table, "name", std::string> name;
  relx::schema::column<test_table, "is_active", bool> is_active;

  // Optional columns for testing
  relx::schema::column<test_table, "optional_id", std::optional<int>> optional_id;
  relx::schema::column<test_table, "optional_name", std::optional<std::string>> optional_name;
  relx::schema::column<test_table, "optional_price", std::optional<double>> optional_price;
};

// Table for testing column-to-column comparisons
struct compatible_table {
  static constexpr auto table_name = "compatible_table";
  relx::schema::column<compatible_table, "id", int> id;
  relx::schema::column<compatible_table, "name", std::string> name;
};

TEST(TypeSafetyTest, ValidComparisons) {
  test_table t;

  // These should all compile - valid comparisons
  auto query1 = relx::query::select(t.id).from(t).where(t.id == 42);  // int column with int value

  auto query2 = relx::query::select(t.price).from(t).where(
      t.price > 10.5);  // double column with double value

  auto query3 = relx::query::select(t.name).from(t).where(
      t.name == "test");  // string column with string literal

  auto query4 = relx::query::select(t.name).from(t).where(
      t.name == std::string("test"));  // string column with std::string

  auto query5 = relx::query::select(t.is_active)
                    .from(t)
                    .where(t.is_active == true);  // bool column with bool value

  // Test string compatibility
  std::string test_str = "test";
  auto query6 = relx::query::select(t.name).from(t).where(t.name == test_str);

  std::string_view test_sv = "test";
  auto query7 = relx::query::select(t.name).from(t).where(t.name == test_sv);

  EXPECT_TRUE(true);  // If we get here, all valid comparisons compiled
}

TEST(TypeSafetyTest, OptionalTypeComparisons) {
  test_table t;

  // Optional column with underlying type - should compile
  auto query1 = relx::query::select(t.optional_id)
                    .from(t)
                    .where(t.optional_id == 42);  // optional<int> with int

  auto query2 = relx::query::select(t.optional_price)
                    .from(t)
                    .where(t.optional_price > 10.5);  // optional<double> with double

  // Non-optional column with optional value - should compile
  std::optional<int> opt_int = 42;
  auto query3 = relx::query::select(t.id).from(t).where(t.id == opt_int);  // int with optional<int>

  // Optional string with string-like types - should compile
  auto query4 = relx::query::select(t.optional_name)
                    .from(t)
                    .where(t.optional_name == "test");  // optional<string> with const char*

  auto query5 = relx::query::select(t.optional_name)
                    .from(t)
                    .where(t.optional_name == std::string("test"));  // optional<string> with string

  std::string_view test_sv = "test";
  auto query6 = relx::query::select(t.optional_name)
                    .from(t)
                    .where(t.optional_name == test_sv);  // optional<string> with string_view

  // String column with optional string - should compile
  std::optional<std::string> opt_str = "test";
  auto query7 = relx::query::select(t.name).from(t).where(t.name ==
                                                          opt_str);  // string with optional<string>

  // Optional to optional - should compile
  std::optional<int> opt_int2 = 123;
  auto query8 = relx::query::select(t.optional_id)
                    .from(t)
                    .where(t.optional_id == opt_int2);  // optional<int> with optional<int>

  std::optional<std::string> opt_str2 = "test";
  auto query9 = relx::query::select(t.optional_name)
                    .from(t)
                    .where(t.optional_name == opt_str2);  // optional<string> with optional<string>

  EXPECT_TRUE(true);  // If we get here, all optional comparisons compiled
}

TEST(TypeSafetyTest, CrossTypeComparisons) {
  test_table t;

  // Test if the library allows potentially problematic cross-type comparisons
  // These might be allowed due to implicit conversions, but we want to understand the behavior

  // THESE ARE INTENTIONALLY BLOCKED BY OUR TYPE CHECKING:
  // Int to double - this is now blocked by type checking
  // auto query1 = relx::query::select(t.id)
  //     .from(t)
  //     .where(t.id == 42.0);  // int column with double value

  // Double to int - this is now blocked by type checking
  // auto query2 = relx::query::select(t.price)
  //     .from(t)
  //     .where(t.price == 42);  // double column with int value

  // String to numeric - this should probably be allowed since SQL often does implicit conversions
  auto query3 = relx::query::select(t.name).from(t).where(
      t.name == "42");  // string column with string value that looks like a number

  // Only test the valid ones
  EXPECT_NO_THROW({ auto sql3 = query3.to_sql(); });
}

// Now test invalid comparisons - these should fail at compile time
TEST(TypeSafetyTest, InvalidComparisons) {
  test_table t;

  // Test some valid ones first
  auto query1 = relx::query::select(t.id).from(t).where(t.id == 42);

  // UNCOMMENT THE FOLLOWING TO TEST TYPE CHECKING:
  // These should all cause compile-time errors:

  // auto invalid1 = relx::query::select(t.id)
  //     .from(t)
  //     .where(t.id == "not_a_number");  // int column with string

  // auto invalid2 = relx::query::select(t.price)
  //     .from(t)
  //     .where(t.price == true);  // double column with bool

  // auto invalid3 = relx::query::select(t.name)
  //     .from(t)
  //     .where(t.name == 42);  // string column with int

  // auto invalid4 = relx::query::select(t.is_active)
  //     .from(t)
  //     .where(t.is_active == 1);  // bool column with int

  // // Invalid optional comparisons
  // auto invalid5 = relx::query::select(t.optional_id)
  //     .from(t)
  //     .where(t.optional_id == "not_a_number");  // optional<int> with string

  // auto invalid6 = relx::query::select(t.optional_name)
  //     .from(t)
  //     .where(t.optional_name == 42);  // optional<string> with int

  EXPECT_TRUE(true);  // If we get here, the test compiled
}

TEST(TypeSafetyTest, AggregateFunctionTypeChecking) {
  test_table t;

  // These should compile - valid aggregate uses
  auto valid_sum = relx::query::select_expr(relx::query::sum(t.id)).from(t);  // int column

  auto valid_avg = relx::query::select_expr(relx::query::avg(t.price)).from(t);  // double column

  auto valid_min_max = relx::query::select_expr(relx::query::min(t.id),
                                                relx::query::max(t.name))
                           .from(t);  // int and string columns

  auto valid_count = relx::query::select_expr(relx::query::count(t.is_active))
                         .from(t);  // bool column (COUNT works on any type)

  // Test string functions
  auto valid_lower = relx::query::select_expr(relx::query::lower(t.name)).from(t);  // string column

  // THESE SHOULD FAIL AT COMPILE TIME (uncomment to test):
  // auto invalid_sum_bool = relx::query::select_expr(relx::query::sum(t.is_active))
  //     .from(t);  // Error: SUM cannot be used with bool columns

  // auto invalid_avg_string = relx::query::select_expr(relx::query::avg(t.name))
  //     .from(t);  // Error: AVG cannot be used with string columns

  // auto invalid_lower_int = relx::query::select_expr(relx::query::lower(t.id))
  //     .from(t);  // Error: LOWER cannot be used with int columns

  EXPECT_TRUE(true);  // Test that valid cases compile
}

TEST(TypeSafetyTest, CaseExpressionTypeChecking) {
  test_table t;

  // This should compile - consistent string types
  auto valid_case = relx::query::case_()
                        .when(t.id < 10, "Small")
                        .when(t.id < 100, "Medium")
                        .else_("Large")
                        .build();

  auto query = relx::query::select_expr(t.id,
                                        relx::query::as(std::move(valid_case), "size_category"))
                   .from(t);

  // THESE SHOULD FAIL AT COMPILE TIME (uncomment to test):
  // Mixed types should fail
  // auto invalid_case = relx::query::case_()
  //     .when(t.id < 10, "Small")     // string
  //     .when(t.id < 100, 42)        // int - ERROR!
  //     .else_("Large")              // string
  //     .build();

  EXPECT_TRUE(true);  // Test that valid cases compile
}

TEST(TypeSafetyTest, ColumnToColumnComparison) {
  test_table t1;
  compatible_table t2;

  // This should compile - same types
  auto valid_join = relx::query::select(t1.id, t1.name)
                        .from(t1)
                        .join(t2, relx::query::on(t1.id == t2.id));  // int == int

  // THESE SHOULD FAIL AT COMPILE TIME (uncomment to test):
  // Different types should fail
  // auto invalid_join = relx::query::select(t1.id, t1.name)
  //     .from(t1)
  //     .join(t2, relx::query::on(t1.id == t2.name));  // int == string - ERROR!

  EXPECT_TRUE(true);  // Test that valid cases compile
}

TEST(TypeSafetyTest, ArithmeticOperationsTypeChecking) {
  test_table t;

  // These should compile - valid arithmetic with numeric columns
  auto valid_addition =
      relx::query::select_expr(t.id + t.optional_id).from(t);  // int + optional<int>

  auto valid_price_calc = relx::query::select_expr(t.price * 1.2).from(t);  // double * double

  auto valid_subtraction = relx::query::select_expr(t.price - 10.0).from(t);  // double - double

  auto valid_division = relx::query::select_expr(t.id / 2).from(t);  // int / int

  // THESE SHOULD FAIL AT COMPILE TIME (uncomment to test):
  // String arithmetic should fail
  // auto invalid_string_add = relx::query::select_expr(t.name + t.name)
  //     .from(t);  // Error: Cannot add strings

  // Boolean arithmetic should fail
  // auto invalid_bool_multiply = relx::query::select_expr(t.is_active * 2)
  //     .from(t);  // Error: Cannot multiply boolean

  // Mixed type arithmetic should fail
  // auto invalid_mixed = relx::query::select_expr(t.id + t.name)
  //     .from(t);  // Error: Cannot add int and string

  EXPECT_TRUE(true);  // Test that valid cases compile
}

TEST(TypeSafetyTest, UpdateAssignmentTypeChecking) {
  test_table t;

  // These should compile - valid assignments
  auto valid_update1 = relx::query::update(t)
                           .set(t.id, 42)                // int column with int value
                           .set(t.name, "Updated Name")  // string column with string value
                           .set(t.price, 99.99);         // double column with double value

  auto valid_update2 = relx::query::update(t)
                           .set(t.is_active, true)    // bool column with bool value
                           .set(t.optional_id, 123);  // optional<int> with int value

  // THESE SHOULD FAIL AT COMPILE TIME (uncomment to test):
  // Type mismatches should fail
  // auto invalid_update1 = relx::query::update(t)
  //     .set(t.id, "not a number");  // Error: int column with string value

  // auto invalid_update2 = relx::query::update(t)
  //     .set(t.price, true);  // Error: double column with bool value

  // auto invalid_update3 = relx::query::update(t)
  //     .set(t.name, 42);  // Error: string column with int value

  EXPECT_TRUE(true);  // Test that valid cases compile
}

TEST(TypeSafetyTest, OrderByTypeChecking) {
  test_table t;

  // These should compile - valid ORDER BY columns
  auto valid_order1 = relx::query::select(t.id, t.name).from(t).order_by(t.id);  // int column

  auto valid_order2 =
      relx::query::select(t.name, t.price).from(t).order_by(t.name);  // string column

  auto valid_order3 = relx::query::select(t.price).from(t).order_by(t.price);  // double column

  // THESE SHOULD FAIL AT COMPILE TIME (uncomment to test):
  // Complex types should fail for ORDER BY
  // auto invalid_order = relx::query::select(t.id)
  //     .from(t)
  //     .order_by(t.is_active);  // Error: bool columns are not comparable for ordering

  EXPECT_TRUE(true);  // Test that valid cases compile
}
