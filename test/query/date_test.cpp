#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>
#include <chrono>
#include <optional>
#include <iostream>

using namespace relx;
using namespace relx::query;

// Test table with date/time columns
struct Employee {
    static constexpr auto table_name = "employees";
    
    schema::column<Employee, "id", int> id;
    schema::column<Employee, "name", std::string> name;
    schema::column<Employee, "hire_date", std::chrono::system_clock::time_point> hire_date;
    schema::column<Employee, "birth_date", std::chrono::system_clock::time_point> birth_date;
    schema::column<Employee, "last_review", std::optional<std::chrono::system_clock::time_point>> last_review;
    schema::column<Employee, "termination_date", std::optional<std::chrono::system_clock::time_point>> termination_date;
    
    schema::pk<&Employee::id> primary;
};

// Test table without date columns for negative testing
struct Product {
    static constexpr auto table_name = "products";
    
    schema::column<Product, "id", int> id;
    schema::column<Product, "name", std::string> name;
    schema::column<Product, "price", double> price;
    schema::column<Product, "is_active", bool> is_active;
    
    schema::pk<&Product::id> primary;
};

class DateFunctionTest : public ::testing::Test {
protected:
    Employee emp;
    Product prod;
};

// Test 1: DATE_DIFF function with various units
TEST_F(DateFunctionTest, DateDiffFunction) {
    // Test date difference between two date columns
    auto query1 = select_expr(date_diff("day", emp.hire_date, emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(query1.to_sql(), "SELECT DATE_DIFF('day', employees.hire_date, employees.birth_date) FROM employees");
    EXPECT_TRUE(query1.bind_params().empty());
    
    // Test with different units
    auto query_years = select_expr(date_diff("year", emp.birth_date, current_date()))
        .from(emp);
    
    EXPECT_EQ(query_years.to_sql(), "SELECT DATE_DIFF('year', employees.birth_date, CURRENT_DATE) FROM employees");
    
    auto query_months = select_expr(date_diff("month", emp.hire_date, current_date()))
        .from(emp);
    
    EXPECT_EQ(query_months.to_sql(), "SELECT DATE_DIFF('month', employees.hire_date, CURRENT_DATE) FROM employees");
    
    auto query_hours = select_expr(date_diff("hour", emp.last_review, current_timestamp()))
        .from(emp);
    
    EXPECT_EQ(query_hours.to_sql(), "SELECT DATE_DIFF('hour', employees.last_review, CURRENT_TIMESTAMP) FROM employees");
}

// Test 2: DATE_ADD and DATE_SUB functions
TEST_F(DateFunctionTest, DateAddSubFunctions) {
    // Test date addition
    auto add_query = select_expr(date_add(emp.hire_date, interval("1 year")))
        .from(emp);
    
    EXPECT_EQ(add_query.to_sql(), "SELECT (employees.hire_date + INTERVAL '1 year') FROM employees");
    EXPECT_TRUE(add_query.bind_params().empty());
    
    // Test date subtraction
    auto sub_query = select_expr(date_sub(emp.hire_date, interval("6 months")))
        .from(emp);
    
    EXPECT_EQ(sub_query.to_sql(), "SELECT (employees.hire_date - INTERVAL '6 months') FROM employees");
    
    // Test with various intervals
    auto day_add = select_expr(date_add(emp.hire_date, interval("30 days")))
        .from(emp);
    
    EXPECT_EQ(day_add.to_sql(), "SELECT (employees.hire_date + INTERVAL '30 days') FROM employees");
    
    auto week_sub = select_expr(date_sub(current_date(), interval("2 weeks")))
        .from(emp);
    
    EXPECT_EQ(week_sub.to_sql(), "SELECT (CURRENT_DATE - INTERVAL '2 weeks') FROM employees");
}

// Test 3: EXTRACT function with various date parts
TEST_F(DateFunctionTest, ExtractFunction) {
    // Test extracting year
    auto year_query = select_expr(extract("year", emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(year_query.to_sql(), "SELECT EXTRACT(year FROM employees.birth_date) FROM employees");
    EXPECT_TRUE(year_query.bind_params().empty());
    
    // Test extracting month
    auto month_query = select_expr(extract("month", emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(month_query.to_sql(), "SELECT EXTRACT(month FROM employees.hire_date) FROM employees");
    
    // Test extracting day
    auto day_query = select_expr(extract("day", emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(day_query.to_sql(), "SELECT EXTRACT(day FROM employees.hire_date) FROM employees");
    
    // Test extracting day of week
    auto dow_query = select_expr(extract("dow", emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(dow_query.to_sql(), "SELECT EXTRACT(dow FROM employees.birth_date) FROM employees");
    
    // Test extracting hour, minute, second from timestamp
    auto hour_query = select_expr(extract("hour", emp.last_review))
        .from(emp);
    
    EXPECT_EQ(hour_query.to_sql(), "SELECT EXTRACT(hour FROM employees.last_review) FROM employees");
    
    auto minute_query = select_expr(extract("minute", emp.last_review))
        .from(emp);
    
    EXPECT_EQ(minute_query.to_sql(), "SELECT EXTRACT(minute FROM employees.last_review) FROM employees");
    
    auto second_query = select_expr(extract("second", emp.last_review))
        .from(emp);
    
    EXPECT_EQ(second_query.to_sql(), "SELECT EXTRACT(second FROM employees.last_review) FROM employees");
}

// Test 4: DATE_TRUNC function
TEST_F(DateFunctionTest, DateTruncFunction) {
    // Test truncating to year
    auto year_trunc = select_expr(date_trunc("year", emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(year_trunc.to_sql(), "SELECT DATE_TRUNC('year', employees.hire_date) FROM employees");
    EXPECT_TRUE(year_trunc.bind_params().empty());
    
    // Test truncating to month
    auto month_trunc = select_expr(date_trunc("month", emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(month_trunc.to_sql(), "SELECT DATE_TRUNC('month', employees.birth_date) FROM employees");
    
    // Test truncating to day
    auto day_trunc = select_expr(date_trunc("day", emp.last_review))
        .from(emp);
    
    EXPECT_EQ(day_trunc.to_sql(), "SELECT DATE_TRUNC('day', employees.last_review) FROM employees");
    
    // Test truncating to hour
    auto hour_trunc = select_expr(date_trunc("hour", current_timestamp()))
        .from(emp);
    
    EXPECT_EQ(hour_trunc.to_sql(), "SELECT DATE_TRUNC('hour', CURRENT_TIMESTAMP) FROM employees");
}

// Test 5: Current date/time functions
TEST_F(DateFunctionTest, CurrentDateTimeFunctions) {
    // Test CURRENT_DATE
    auto current_date_query = select_expr(current_date())
        .from(emp);
    
    EXPECT_EQ(current_date_query.to_sql(), "SELECT CURRENT_DATE FROM employees");
    EXPECT_TRUE(current_date_query.bind_params().empty());
    
    // Test CURRENT_TIME
    auto current_time_query = select_expr(current_time())
        .from(emp);
    
    EXPECT_EQ(current_time_query.to_sql(), "SELECT CURRENT_TIME FROM employees");
    
    // Test CURRENT_TIMESTAMP
    auto current_timestamp_query = select_expr(current_timestamp())
        .from(emp);
    
    EXPECT_EQ(current_timestamp_query.to_sql(), "SELECT CURRENT_TIMESTAMP FROM employees");
    
    // Test NOW() function
    auto now_query = select_expr(now())
        .from(emp);
    
    EXPECT_EQ(now_query.to_sql(), "SELECT NOW() FROM employees");
}

// Test 6: Helper functions for common operations
TEST_F(DateFunctionTest, HelperFunctions) {
    // Test age_in_years helper
    auto age_query = select_expr(age_in_years(emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(age_query.to_sql(), "SELECT DATE_DIFF('year', employees.birth_date, CURRENT_DATE) FROM employees");
    
    // Test days_since helper
    auto days_since_query = select_expr(days_since(emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(days_since_query.to_sql(), "SELECT DATE_DIFF('day', employees.hire_date, CURRENT_DATE) FROM employees");
    
    // Test days_until helper
    auto days_until_query = select_expr(days_until(emp.termination_date))
        .from(emp);
    
    EXPECT_EQ(days_until_query.to_sql(), "SELECT DATE_DIFF('day', CURRENT_DATE, employees.termination_date) FROM employees");
    
    // Test start_of_year helper
    auto start_year = select_expr(start_of_year(emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(start_year.to_sql(), "SELECT DATE_TRUNC('year', employees.hire_date) FROM employees");
    
    // Test start_of_month helper
    auto start_month = select_expr(start_of_month(emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(start_month.to_sql(), "SELECT DATE_TRUNC('month', employees.hire_date) FROM employees");
    
    // Test start_of_day helper
    auto start_day = select_expr(start_of_day(emp.last_review))
        .from(emp);
    
    EXPECT_EQ(start_day.to_sql(), "SELECT DATE_TRUNC('day', employees.last_review) FROM employees");
}

// Test 7: Extract helper functions
TEST_F(DateFunctionTest, ExtractHelperFunctions) {
    // Test year() helper
    auto year_query = select_expr(year(emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(year_query.to_sql(), "SELECT EXTRACT(year FROM employees.birth_date) FROM employees");
    
    // Test month() helper
    auto month_query = select_expr(month(emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(month_query.to_sql(), "SELECT EXTRACT(month FROM employees.hire_date) FROM employees");
    
    // Test day() helper
    auto day_query = select_expr(day(emp.hire_date))
        .from(emp);
    
    EXPECT_EQ(day_query.to_sql(), "SELECT EXTRACT(day FROM employees.hire_date) FROM employees");
    
    // Test day_of_week() helper
    auto dow_query = select_expr(day_of_week(emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(dow_query.to_sql(), "SELECT EXTRACT(dow FROM employees.birth_date) FROM employees");
    
    // Test day_of_year() helper
    auto doy_query = select_expr(day_of_year(emp.birth_date))
        .from(emp);
    
    EXPECT_EQ(doy_query.to_sql(), "SELECT EXTRACT(doy FROM employees.birth_date) FROM employees");
    
    // Test hour() helper
    auto hour_query = select_expr(hour(emp.last_review))
        .from(emp);
    
    EXPECT_EQ(hour_query.to_sql(), "SELECT EXTRACT(hour FROM employees.last_review) FROM employees");
    
    // Test minute() helper
    auto minute_query = select_expr(minute(emp.last_review))
        .from(emp);
    
    EXPECT_EQ(minute_query.to_sql(), "SELECT EXTRACT(minute FROM employees.last_review) FROM employees");
    
    // Test second() helper
    auto second_query = select_expr(second(emp.last_review))
        .from(emp);
    
    EXPECT_EQ(second_query.to_sql(), "SELECT EXTRACT(second FROM employees.last_review) FROM employees");
}

// Test 8: Complex queries with date functions
TEST_F(DateFunctionTest, ComplexDateQueries) {
    // Query with multiple date operations
    auto complex_query = select(
        emp.id,
        emp.name,
        as(age_in_years(emp.birth_date), "age"),
        as(days_since(emp.hire_date), "tenure_days"),
        as(year(emp.hire_date), "hire_year"),
        as(month(emp.hire_date), "hire_month")
    )
    .from(emp)
    .where(age_in_years(emp.birth_date) >= 21)
    .order_by(days_since(emp.hire_date));
    
    std::string expected_sql = 
        "SELECT employees.id, employees.name, "
        "DATE_DIFF('year', employees.birth_date, CURRENT_DATE) AS age, "
        "DATE_DIFF('day', employees.hire_date, CURRENT_DATE) AS tenure_days, "
        "EXTRACT(year FROM employees.hire_date) AS hire_year, "
        "EXTRACT(month FROM employees.hire_date) AS hire_month "
        "FROM employees "
        "WHERE (DATE_DIFF('year', employees.birth_date, CURRENT_DATE) >= ?) "
        "ORDER BY DATE_DIFF('day', employees.hire_date, CURRENT_DATE)";
    
    EXPECT_EQ(complex_query.to_sql(), expected_sql);
    
    auto params = complex_query.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "21");
}

// Test 9: Date functions in WHERE clauses
TEST_F(DateFunctionTest, DateFunctionsInWhere) {
    // Filter by employees hired in the last year
    auto recent_hires = select(emp.id, emp.name)
        .from(emp)
        .where(date_diff("day", emp.hire_date, current_date()) <= 365);
    
    EXPECT_EQ(recent_hires.to_sql(), 
        "SELECT employees.id, employees.name FROM employees "
        "WHERE (DATE_DIFF('day', employees.hire_date, CURRENT_DATE) <= ?)");
    
    auto params = recent_hires.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "365");
    
    // Filter by birth year
    auto born_in_1990 = select(emp.id, emp.name)
        .from(emp)
        .where(year(emp.birth_date) == 1990);
    
    EXPECT_EQ(born_in_1990.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE (EXTRACT(year FROM employees.birth_date) = ?)");
    
    params = born_in_1990.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "1990");
    
    // Filter by employees who have reviews scheduled
    auto has_review = select(emp.id, emp.name)
        .from(emp)
        .where(emp.last_review.is_not_null());
    
    EXPECT_EQ(has_review.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE employees.last_review IS NOT NULL");
}

// Test 10: Date functions with ORDER BY
TEST_F(DateFunctionTest, DateFunctionsInOrderBy) {
    // Order by age (youngest first)
    auto order_by_age = select(emp.id, emp.name)
        .from(emp)
        .order_by(desc(age_in_years(emp.birth_date)));
    
    EXPECT_EQ(order_by_age.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "ORDER BY DATE_DIFF('year', employees.birth_date, CURRENT_DATE) DESC");
    
    // Order by hire date (most recent first)
    auto order_by_hire = select(emp.id, emp.name)
        .from(emp)
        .order_by(desc(emp.hire_date), asc(emp.name));
    
    EXPECT_EQ(order_by_hire.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "ORDER BY employees.hire_date DESC, employees.name ASC");
}

// Test 11: Date functions with GROUP BY and aggregates
TEST_F(DateFunctionTest, DateFunctionsWithGroupBy) {
    // Group by hire year and count employees
    auto by_hire_year = select_expr(
        year(emp.hire_date),
        as(count_all(), "employee_count")
    )
    .from(emp)
    .group_by(year(emp.hire_date))
    .order_by(year(emp.hire_date));
    
    EXPECT_EQ(by_hire_year.to_sql(),
        "SELECT EXTRACT(year FROM employees.hire_date), COUNT(*) AS employee_count "
        "FROM employees "
        "GROUP BY EXTRACT(year FROM employees.hire_date) "
        "ORDER BY EXTRACT(year FROM employees.hire_date)");
    
    // Group by quarter (using month truncation)
    auto by_quarter = select_expr(
        date_trunc("quarter", emp.hire_date),
        as(count_all(), "hires_per_quarter")
    )
    .from(emp)
    .group_by(date_trunc("quarter", emp.hire_date))
    .order_by(date_trunc("quarter", emp.hire_date));
    
    EXPECT_EQ(by_quarter.to_sql(),
        "SELECT DATE_TRUNC('quarter', employees.hire_date), COUNT(*) AS hires_per_quarter "
        "FROM employees "
        "GROUP BY DATE_TRUNC('quarter', employees.hire_date) "
        "ORDER BY DATE_TRUNC('quarter', employees.hire_date)");
}

// Test 12: Interval expressions
TEST_F(DateFunctionTest, IntervalExpressions) {
    // Test various interval formats
    auto intervals = select_expr(
        interval("1 day"),
        interval("2 weeks"),
        interval("3 months"),
        interval("1 year"),
        interval("5 hours"),
        interval("30 minutes")
    )
    .from(emp);
    
    EXPECT_EQ(intervals.to_sql(),
        "SELECT INTERVAL '1 day', INTERVAL '2 weeks', INTERVAL '3 months', "
        "INTERVAL '1 year', INTERVAL '5 hours', INTERVAL '30 minutes' "
        "FROM employees");
}

// Test 13: Mixed optional and non-optional date columns
TEST_F(DateFunctionTest, OptionalDateColumns) {
    // Test with optional date column
    auto optional_date_query = select_expr(
        emp.id,
        emp.name,
        as(days_since(emp.last_review), "days_since_review"),
        as(date_add(emp.last_review, interval("1 year")), "next_review_due")
    )
    .from(emp)
    .where(emp.last_review.is_not_null());
    
    EXPECT_EQ(optional_date_query.to_sql(),
        "SELECT employees.id, employees.name, "
        "DATE_DIFF('day', employees.last_review, CURRENT_DATE) AS days_since_review, "
        "(employees.last_review + INTERVAL '1 year') AS next_review_due "
        "FROM employees "
        "WHERE employees.last_review IS NOT NULL");
    
    // Test optional date in comparison
    auto termination_query = select(emp.id, emp.name)
        .from(emp)
        .where(emp.termination_date.is_null());
    
    EXPECT_EQ(termination_query.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE employees.termination_date IS NULL");
}

// Test 14: Type safety compilation tests
TEST_F(DateFunctionTest, TypeSafetyCompilation) {
    // These should compile - valid date operations
    auto valid_ops = select_expr(
        date_diff("day", emp.hire_date, emp.birth_date),
        extract("year", emp.birth_date),
        date_add(emp.hire_date, interval("1 year")),
        age_in_years(emp.birth_date),
        year(emp.hire_date)
    ).from(emp);
    
    // Test that the query compiles and generates expected SQL
    EXPECT_FALSE(valid_ops.to_sql().empty());
    
    // THESE WOULD FAIL AT COMPILE TIME (uncomment to test):
    // auto invalid_date_diff = date_diff("day", prod.id, prod.name);  // Error: not date columns
    // auto invalid_extract = extract("year", prod.price);            // Error: not date column
    // auto invalid_date_add = date_add(prod.name, interval("1 day")); // Error: not date column
    // auto invalid_age = age_in_years(prod.is_active);               // Error: not date column
    
    EXPECT_TRUE(true); // Test that valid cases compile
}

// Test 15: Date arithmetic with current functions
TEST_F(DateFunctionTest, DateArithmeticWithCurrentFunctions) {
    // Future dates based on current date
    auto future_dates = select_expr(
        as(date_add(current_date(), interval("30 days")), "thirty_days_from_now"),
        as(date_add(current_timestamp(), interval("1 hour")), "one_hour_from_now"),
        as(date_sub(current_date(), interval("1 week")), "one_week_ago")
    )
    .from(emp);
    
    EXPECT_EQ(future_dates.to_sql(),
        "SELECT (CURRENT_DATE + INTERVAL '30 days') AS thirty_days_from_now, "
        "(CURRENT_TIMESTAMP + INTERVAL '1 hour') AS one_hour_from_now, "
        "(CURRENT_DATE - INTERVAL '1 week') AS one_week_ago "
        "FROM employees");
    
    EXPECT_TRUE(future_dates.bind_params().empty());
}

// Test 16: Complex date conditions and business logic
TEST_F(DateFunctionTest, ComplexBusinessLogic) {
    // Find employees eligible for retirement (65+ years old with 10+ years tenure)
    auto retirement_eligible = select(emp.id, emp.name)
        .from(emp)
        .where(
            (age_in_years(emp.birth_date) >= 65) &&
            (date_diff("year", emp.hire_date, current_date()) >= 10)
        );
    
    EXPECT_EQ(retirement_eligible.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE ((DATE_DIFF('year', employees.birth_date, CURRENT_DATE) >= ?) AND "
        "(DATE_DIFF('year', employees.hire_date, CURRENT_DATE) >= ?))");
    
    auto params = retirement_eligible.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "65");
    EXPECT_EQ(params[1], "10");
    
    // Find employees with upcoming work anniversaries (hired in current month)
    auto anniversary_this_month = select(emp.id, emp.name, emp.hire_date)
        .from(emp)
        .where(
            (month(emp.hire_date) == month(current_date())) &&
            (day(emp.hire_date) >= day(current_date()))
        );
    
    EXPECT_EQ(anniversary_this_month.to_sql(),
        "SELECT employees.id, employees.name, employees.hire_date FROM employees "
        "WHERE ((EXTRACT(month FROM employees.hire_date) = EXTRACT(month FROM CURRENT_DATE)) AND "
        "(EXTRACT(day FROM employees.hire_date) >= EXTRACT(day FROM CURRENT_DATE)))");
}

// Test 17: Helper functions work with both columns and expressions
TEST_F(DateFunctionTest, HelperFunctionsWithExpressions) {
    // Test that helper functions work cleanly with both columns and current date functions
    auto clean_syntax_query = select_expr(
        as(year(emp.hire_date), "hire_year"),
        as(month(emp.hire_date), "hire_month"), 
        as(day(emp.hire_date), "hire_day"),
        as(year(current_date()), "current_year"),
        as(month(current_date()), "current_month"),
        as(day(current_date()), "current_day"),
        as(hour(current_timestamp()), "current_hour"),
        as(minute(now()), "current_minute")
    )
    .from(emp)
    .where(
        (year(emp.hire_date) == year(current_date())) &&
        (month(emp.hire_date) <= month(current_date()))
    );
    
    EXPECT_EQ(clean_syntax_query.to_sql(),
        "SELECT "
        "EXTRACT(year FROM employees.hire_date) AS hire_year, "
        "EXTRACT(month FROM employees.hire_date) AS hire_month, "
        "EXTRACT(day FROM employees.hire_date) AS hire_day, "
        "EXTRACT(year FROM CURRENT_DATE) AS current_year, "
        "EXTRACT(month FROM CURRENT_DATE) AS current_month, "
        "EXTRACT(day FROM CURRENT_DATE) AS current_day, "
        "EXTRACT(hour FROM CURRENT_TIMESTAMP) AS current_hour, "
        "EXTRACT(minute FROM NOW()) AS current_minute "
        "FROM employees "
        "WHERE ((EXTRACT(year FROM employees.hire_date) = EXTRACT(year FROM CURRENT_DATE)) AND "
        "(EXTRACT(month FROM employees.hire_date) <= EXTRACT(month FROM CURRENT_DATE)))");
    
    // Test comparison - old verbose way vs new clean way
    // Old way (still works but verbose):
    auto verbose_query = select(emp.id, emp.name)
        .from(emp)
        .where(extract("month", emp.hire_date) == extract("month", current_date()));
    
    // New clean way:
    auto clean_query = select(emp.id, emp.name)
        .from(emp)
        .where(month(emp.hire_date) == month(current_date()));
    
    // Both should generate the same SQL
    EXPECT_EQ(verbose_query.to_sql(), clean_query.to_sql());
    EXPECT_EQ(clean_query.to_sql(), 
        "SELECT employees.id, employees.name FROM employees "
        "WHERE (EXTRACT(month FROM employees.hire_date) = EXTRACT(month FROM CURRENT_DATE))");
}

// Test 18: Complex nested and compound date function compositions
TEST_F(DateFunctionTest, ComplexNestedDateFunctionCompositions) {
    // Test 1: Deeply nested date arithmetic and extractions
    // Find employees hired in the 6th month after their birthday year started
    auto complex_nested_1 = select(emp.id, emp.name)
        .from(emp)
        .where(
            month(date_add(start_of_year(emp.birth_date), interval("6 months"))) == 
            month(emp.hire_date)
        );
    
    EXPECT_EQ(complex_nested_1.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE (EXTRACT(month FROM (DATE_TRUNC('year', employees.birth_date) + INTERVAL '6 months')) = "
        "EXTRACT(month FROM employees.hire_date))");
    
    // Test 2: Multi-level function composition with current date
    // Employees hired in the same month as 3 years before current date
    auto complex_nested_2 = select_expr(
        emp.id,
        emp.name,
        as(year(date_sub(current_date(), interval("3 years"))), "three_years_ago_year"),
        as(month(date_sub(current_date(), interval("3 years"))), "three_years_ago_month")
    )
    .from(emp)
    .where(
        (month(emp.hire_date) == month(date_sub(current_date(), interval("3 years")))) &&
        (year(emp.hire_date) >= year(date_sub(current_date(), interval("10 years"))))
    );
    
    EXPECT_EQ(complex_nested_2.to_sql(),
        "SELECT employees.id, employees.name, "
        "EXTRACT(year FROM (CURRENT_DATE - INTERVAL '3 years')) AS three_years_ago_year, "
        "EXTRACT(month FROM (CURRENT_DATE - INTERVAL '3 years')) AS three_years_ago_month "
        "FROM employees "
        "WHERE ((EXTRACT(month FROM employees.hire_date) = EXTRACT(month FROM (CURRENT_DATE - INTERVAL '3 years'))) AND "
        "(EXTRACT(year FROM employees.hire_date) >= EXTRACT(year FROM (CURRENT_DATE - INTERVAL '10 years'))))");
    
    // Test 3: Ultra-complex business logic with multiple nested operations
    // Find employees who:
    // 1. Were hired in the first quarter of their birth year + 25 years
    // 2. Have their next review due (hire date + 1 year) in the current quarter
    auto ultra_complex = select_expr(
        emp.id,
        emp.name,
        as(date_add(start_of_year(emp.birth_date), interval("25 years")), "target_year_start"),
        as(date_add(emp.hire_date, interval("1 year")), "next_review_due"),
        as(date_trunc("quarter", current_date()), "current_quarter_start")
    )
    .from(emp)
    .where(
        // Hired in first quarter of (birth year + 25)
        (date_trunc("quarter", emp.hire_date) == 
         date_trunc("quarter", date_add(start_of_year(emp.birth_date), interval("25 years")))) &&
        // Next review due in current quarter
        (date_trunc("quarter", date_add(emp.hire_date, interval("1 year"))) == 
         date_trunc("quarter", current_date())) &&
        // Additional complexity: Day of hire should be in first half of month
        (day(emp.hire_date) <= 15)
    );
    
    EXPECT_EQ(ultra_complex.to_sql(),
        "SELECT employees.id, employees.name, "
        "(DATE_TRUNC('year', employees.birth_date) + INTERVAL '25 years') AS target_year_start, "
        "(employees.hire_date + INTERVAL '1 year') AS next_review_due, "
        "DATE_TRUNC('quarter', CURRENT_DATE) AS current_quarter_start "
        "FROM employees "
        "WHERE (((DATE_TRUNC('quarter', employees.hire_date) = "
        "DATE_TRUNC('quarter', (DATE_TRUNC('year', employees.birth_date) + INTERVAL '25 years'))) AND "
        "(DATE_TRUNC('quarter', (employees.hire_date + INTERVAL '1 year')) = "
        "DATE_TRUNC('quarter', CURRENT_DATE))) AND "
        "(EXTRACT(day FROM employees.hire_date) <= ?))");
    
    auto params = ultra_complex.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "15");
    
    // Test 4: Recursive-style composition for complex age calculations
    // Employees whose age in years times 2 equals the year they were hired relative to epoch
    auto recursive_complex = select_expr(
        emp.id,
        emp.name,
        as(age_in_years(emp.birth_date), "current_age"),
        as(year(emp.hire_date), "hire_year"),
        as(date_diff("year", 
            date_trunc("year", date_sub(current_date(), interval("50 years"))), // 50 years ago as "epoch"
            start_of_year(emp.hire_date)
        ), "years_since_epoch")
    )
    .from(emp)
    .where(
        (age_in_years(emp.birth_date) * 2) == 
        date_diff("year", 
            date_trunc("year", date_sub(current_date(), interval("50 years"))),
            start_of_year(emp.hire_date)
        )
    );
    
    EXPECT_EQ(recursive_complex.to_sql(),
        "SELECT employees.id, employees.name, "
        "DATE_DIFF('year', employees.birth_date, CURRENT_DATE) AS current_age, "
        "EXTRACT(year FROM employees.hire_date) AS hire_year, "
        "DATE_DIFF('year', DATE_TRUNC('year', (CURRENT_DATE - INTERVAL '50 years')), "
        "DATE_TRUNC('year', employees.hire_date)) AS years_since_epoch "
        "FROM employees "
        "WHERE ((DATE_DIFF('year', employees.birth_date, CURRENT_DATE) * ?) = "
        "DATE_DIFF('year', DATE_TRUNC('year', (CURRENT_DATE - INTERVAL '50 years')), "
        "DATE_TRUNC('year', employees.hire_date)))");
    
    params = recursive_complex.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "2");
    
    // Test 5: Time-based nested operations with current timestamp
    // Complex timestamp operations with nested hour/minute extractions
    auto timestamp_nested = select_expr(
        emp.id,
        as(hour(date_add(current_timestamp(), interval("3 hours"))), "future_hour"),
        as(minute(date_sub(now(), interval("30 minutes"))), "past_minute"),
        as(second(current_timestamp()), "current_second")
    )
    .from(emp)
    .where(
        // Employees with reviews in the morning (before noon) of next day
        (hour(date_add(emp.last_review, interval("1 day"))) < 12) &&
        // And the review minute matches current minute 
        (minute(emp.last_review) == minute(current_time()))
    );
    
    EXPECT_EQ(timestamp_nested.to_sql(),
        "SELECT employees.id, "
        "EXTRACT(hour FROM (CURRENT_TIMESTAMP + INTERVAL '3 hours')) AS future_hour, "
        "EXTRACT(minute FROM (NOW() - INTERVAL '30 minutes')) AS past_minute, "
        "EXTRACT(second FROM CURRENT_TIMESTAMP) AS current_second "
        "FROM employees "
        "WHERE ((EXTRACT(hour FROM (employees.last_review + INTERVAL '1 day')) < ?) AND "
        "(EXTRACT(minute FROM employees.last_review) = EXTRACT(minute FROM CURRENT_TIME)))");
    
    params = timestamp_nested.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "12");
    
    // Test 6: Maximum complexity - nested everything
    // The ultimate stress test with multiple levels of every operation
    auto maximum_complexity = select_expr(
        emp.id,
        as(
            day(
                date_add(
                    date_trunc("month", 
                        date_sub(
                            start_of_year(current_date()),
                            interval("1 year")
                        )
                    ),
                    interval("45 days")
                )
            ), 
            "complex_day_calculation"
        )
    )
    .from(emp)
    .where(
        year(
            date_add(
                start_of_month(emp.hire_date), 
                interval("6 months")
            )
        ) == 
        year(
            date_sub(
                current_date(), 
                interval(
                    // This would be dynamic in real usage, but for test we'll use a constant
                    "2 years"
                )
            )
        )
    )
    .order_by(
        month(
            date_add(
                date_trunc("year", emp.birth_date),
                interval("20 years")
            )
        )
    );
    
    EXPECT_EQ(maximum_complexity.to_sql(),
        "SELECT employees.id, "
        "EXTRACT(day FROM (DATE_TRUNC('month', (DATE_TRUNC('year', CURRENT_DATE) - INTERVAL '1 year')) + INTERVAL '45 days')) "
        "AS complex_day_calculation "
        "FROM employees "
        "WHERE (EXTRACT(year FROM (DATE_TRUNC('month', employees.hire_date) + INTERVAL '6 months')) = "
        "EXTRACT(year FROM (CURRENT_DATE - INTERVAL '2 years'))) "
        "ORDER BY EXTRACT(month FROM (DATE_TRUNC('year', employees.birth_date) + INTERVAL '20 years'))");
}

// Test 19: Operator overloads for date arithmetic (+ and - instead of date_add/date_sub)
TEST_F(DateFunctionTest, DateArithmeticOperatorOverloads) {
    // Test 1: Column + interval using operator
    auto col_plus_interval = select_expr(
        emp.id,
        as(emp.hire_date + interval("1 year"), "hire_plus_year"),
        as(emp.birth_date + interval("25 years"), "birth_plus_25")
    )
    .from(emp);
    
    EXPECT_EQ(col_plus_interval.to_sql(),
        "SELECT employees.id, "
        "(employees.hire_date + INTERVAL '1 year') AS hire_plus_year, "
        "(employees.birth_date + INTERVAL '25 years') AS birth_plus_25 "
        "FROM employees");
    
    // Test 2: Column - interval using operator
    auto col_minus_interval = select_expr(
        emp.id,
        as(emp.hire_date - interval("6 months"), "hire_minus_6months"),
        as(current_date() - interval("1 year"), "year_ago")
    )
    .from(emp);
    
    EXPECT_EQ(col_minus_interval.to_sql(),
        "SELECT employees.id, "
        "(employees.hire_date - INTERVAL '6 months') AS hire_minus_6months, "
        "(CURRENT_DATE - INTERVAL '1 year') AS year_ago "
        "FROM employees");
    
    // Test 3: Complex nested operations with operators
    auto complex_operators = select_expr(
        emp.id,
        as(year(emp.hire_date + interval("10 years")), "future_hire_year"),
        as(month(current_date() - interval("2 years")), "past_month"),
        as(day(emp.birth_date + interval("50 years")), "future_birthday_day")
    )
    .from(emp)
    .where(
        (emp.hire_date + interval("5 years")) > current_date() &&
        (emp.birth_date + interval("65 years")) < (current_date() + interval("10 years"))
    );
    
    EXPECT_EQ(complex_operators.to_sql(),
        "SELECT employees.id, "
        "EXTRACT(year FROM (employees.hire_date + INTERVAL '10 years')) AS future_hire_year, "
        "EXTRACT(month FROM (CURRENT_DATE - INTERVAL '2 years')) AS past_month, "
        "EXTRACT(day FROM (employees.birth_date + INTERVAL '50 years')) AS future_birthday_day "
        "FROM employees "
        "WHERE (((employees.hire_date + INTERVAL '5 years') > CURRENT_DATE) AND "
        "((employees.birth_date + INTERVAL '65 years') < (CURRENT_DATE + INTERVAL '10 years')))");
    
    // Test 4: Comparison with explicit function calls - should generate same SQL
    auto explicit_functions = select_expr(
        emp.id,
        as(date_add(emp.hire_date, interval("1 year")), "hire_plus_year")
    )
    .from(emp);
    
    auto operator_version = select_expr(
        emp.id,
        as(emp.hire_date + interval("1 year"), "hire_plus_year")
    )
    .from(emp);
    
    // Both should generate valid SQL with the same structure
    std::string explicit_sql = explicit_functions.to_sql();
    std::string operator_sql = operator_version.to_sql();
    
    // Debug: Let's see the actual strings
    std::cout << "Explicit SQL: '" << explicit_sql << "'" << std::endl;
    std::cout << "Operator SQL: '" << operator_sql << "'" << std::endl;
    
    // Replace alias names to compare the core functionality
    std::string explicit_normalized = explicit_sql;
    std::string operator_normalized = operator_sql;
    
    // Replace the explicit alias name with a common one
    size_t pos = explicit_normalized.find("hire_plus_year_explicit");
    if (pos != std::string::npos) {
        explicit_normalized.replace(pos, 24, "hire_plus_year");
    }
    
    // Replace the operator alias name with the same common one
    pos = operator_normalized.find("hire_plus_year_operator");
    if (pos != std::string::npos) {
        operator_normalized.replace(pos, 23, "hire_plus_year");
    }
    
    std::cout << "Explicit normalized: '" << explicit_normalized << "'" << std::endl;
    std::cout << "Operator normalized: '" << operator_normalized << "'" << std::endl;
    
    EXPECT_EQ(explicit_normalized, operator_normalized);
    
    // Test 5: Multiple arithmetic operations in one expression
    auto multiple_operations = select_expr(
        emp.id,
        as(
            year((emp.hire_date + interval("1 year")) - interval("6 months")), 
            "complex_year_calc"
        ),
        as(
            month(current_date() - interval("3 years") + interval("6 months")),
            "complex_month_calc"
        )
    )
    .from(emp);
    
    EXPECT_EQ(multiple_operations.to_sql(),
        "SELECT employees.id, "
        "EXTRACT(year FROM ((employees.hire_date + INTERVAL '1 year') - INTERVAL '6 months')) AS complex_year_calc, "
        "EXTRACT(month FROM ((CURRENT_DATE - INTERVAL '3 years') + INTERVAL '6 months')) AS complex_month_calc "
        "FROM employees");
}

// Test 20: Comprehensive current_date() compatibility with all date functions
TEST_F(DateFunctionTest, CurrentDateWithAllFunctions) {
    // Test current_date() with date_diff in both positions
    auto date_diff_tests = select_expr(
        as(date_diff("day", current_date(), emp.hire_date), "days_from_current_to_hire"),
        as(date_diff("month", emp.birth_date, current_date()), "months_from_birth_to_current"),
        as(date_diff("year", current_date(), emp.termination_date), "years_from_current_to_termination"),
        as(date_diff("hour", current_date(), current_date()), "should_be_zero_hours")
    )
    .from(emp);
    
    EXPECT_EQ(date_diff_tests.to_sql(),
        "SELECT "
        "DATE_DIFF('day', CURRENT_DATE, employees.hire_date) AS days_from_current_to_hire, "
        "DATE_DIFF('month', employees.birth_date, CURRENT_DATE) AS months_from_birth_to_current, "
        "DATE_DIFF('year', CURRENT_DATE, employees.termination_date) AS years_from_current_to_termination, "
        "DATE_DIFF('hour', CURRENT_DATE, CURRENT_DATE) AS should_be_zero_hours "
        "FROM employees");
    
    // Test current_date() with date_add and date_sub
    auto date_arithmetic_tests = select_expr(
        as(date_add(current_date(), interval("1 year")), "current_plus_year"),
        as(date_sub(current_date(), interval("6 months")), "current_minus_months"),
        as(current_date() + interval("30 days"), "current_plus_operator"),
        as(current_date() - interval("1 week"), "current_minus_operator")
    )
    .from(emp);
    
    EXPECT_EQ(date_arithmetic_tests.to_sql(),
        "SELECT "
        "(CURRENT_DATE + INTERVAL '1 year') AS current_plus_year, "
        "(CURRENT_DATE - INTERVAL '6 months') AS current_minus_months, "
        "(CURRENT_DATE + INTERVAL '30 days') AS current_plus_operator, "
        "(CURRENT_DATE - INTERVAL '1 week') AS current_minus_operator "
        "FROM employees");
    
    // Test current_date() with extract function (all parts)
    auto extract_tests = select_expr(
        as(extract("year", current_date()), "current_year"),
        as(extract("month", current_date()), "current_month"),
        as(extract("day", current_date()), "current_day"),
        as(extract("dow", current_date()), "current_day_of_week"),
        as(extract("doy", current_date()), "current_day_of_year"),
        as(extract("quarter", current_date()), "current_quarter"),
        as(extract("week", current_date()), "current_week")
    )
    .from(emp);
    
    EXPECT_EQ(extract_tests.to_sql(),
        "SELECT "
        "EXTRACT(year FROM CURRENT_DATE) AS current_year, "
        "EXTRACT(month FROM CURRENT_DATE) AS current_month, "
        "EXTRACT(day FROM CURRENT_DATE) AS current_day, "
        "EXTRACT(dow FROM CURRENT_DATE) AS current_day_of_week, "
        "EXTRACT(doy FROM CURRENT_DATE) AS current_day_of_year, "
        "EXTRACT(quarter FROM CURRENT_DATE) AS current_quarter, "
        "EXTRACT(week FROM CURRENT_DATE) AS current_week "
        "FROM employees");
    
    // Test current_date() with date_trunc function (all meaningful parts)
    auto date_trunc_tests = select_expr(
        as(date_trunc("year", current_date()), "current_truncated_to_year"),
        as(date_trunc("quarter", current_date()), "current_truncated_to_quarter"),
        as(date_trunc("month", current_date()), "current_truncated_to_month"),
        as(date_trunc("week", current_date()), "current_truncated_to_week"),
        as(date_trunc("day", current_date()), "current_truncated_to_day")
    )
    .from(emp);
    
    EXPECT_EQ(date_trunc_tests.to_sql(),
        "SELECT "
        "DATE_TRUNC('year', CURRENT_DATE) AS current_truncated_to_year, "
        "DATE_TRUNC('quarter', CURRENT_DATE) AS current_truncated_to_quarter, "
        "DATE_TRUNC('month', CURRENT_DATE) AS current_truncated_to_month, "
        "DATE_TRUNC('week', CURRENT_DATE) AS current_truncated_to_week, "
        "DATE_TRUNC('day', CURRENT_DATE) AS current_truncated_to_day "
        "FROM employees");
    
    // Test current_date() with all helper extract functions
    auto extract_helper_tests = select_expr(
        as(year(current_date()), "current_year_helper"),
        as(month(current_date()), "current_month_helper"),
        as(day(current_date()), "current_day_helper"),
        as(day_of_week(current_date()), "current_dow_helper"),
        as(day_of_year(current_date()), "current_doy_helper")
    )
    .from(emp);
    
    EXPECT_EQ(extract_helper_tests.to_sql(),
        "SELECT "
        "EXTRACT(year FROM CURRENT_DATE) AS current_year_helper, "
        "EXTRACT(month FROM CURRENT_DATE) AS current_month_helper, "
        "EXTRACT(day FROM CURRENT_DATE) AS current_day_helper, "
        "EXTRACT(dow FROM CURRENT_DATE) AS current_dow_helper, "
        "EXTRACT(doy FROM CURRENT_DATE) AS current_doy_helper "
        "FROM employees");
    
    // Test current_date() with all helper trunc functions
    auto trunc_helper_tests = select_expr(
        as(start_of_year(current_date()), "current_start_of_year"),
        as(start_of_month(current_date()), "current_start_of_month"),
        as(start_of_day(current_date()), "current_start_of_day")
    )
    .from(emp);
    
    EXPECT_EQ(trunc_helper_tests.to_sql(),
        "SELECT "
        "DATE_TRUNC('year', CURRENT_DATE) AS current_start_of_year, "
        "DATE_TRUNC('month', CURRENT_DATE) AS current_start_of_month, "
        "DATE_TRUNC('day', CURRENT_DATE) AS current_start_of_day "
        "FROM employees");
    
    // Test current_date() with helper date_diff functions
    auto helper_diff_tests = select_expr(
        as(age_in_years(current_date()), "age_of_current_date"), // Should be 0
        as(days_since(current_date()), "days_since_current"), // Should be 0
        as(days_until(current_date()), "days_until_current") // Should be 0
    )
    .from(emp);
    
    EXPECT_EQ(helper_diff_tests.to_sql(),
        "SELECT "
        "DATE_DIFF('year', CURRENT_DATE, CURRENT_DATE) AS age_of_current_date, "
        "DATE_DIFF('day', CURRENT_DATE, CURRENT_DATE) AS days_since_current, "
        "DATE_DIFF('day', CURRENT_DATE, CURRENT_DATE) AS days_until_current "
        "FROM employees");
    
    // Test current_date() in complex nested expressions
    auto complex_nested_current = select_expr(
        as(
            year(date_add(start_of_year(current_date()), interval("6 months"))),
            "mid_year_of_current"
        ),
        as(
            month(date_sub(start_of_month(current_date()), interval("1 day"))),
            "last_month_from_current"
        ),
        as(
            day(date_trunc("week", current_date()) + interval("3 days")),
            "wednesday_of_current_week"
        ),
        as(
            extract("quarter", current_date() + interval("3 months")),
            "next_quarter_from_current"
        )
    )
    .from(emp);
    
    EXPECT_EQ(complex_nested_current.to_sql(),
        "SELECT "
        "EXTRACT(year FROM (DATE_TRUNC('year', CURRENT_DATE) + INTERVAL '6 months')) AS mid_year_of_current, "
        "EXTRACT(month FROM (DATE_TRUNC('month', CURRENT_DATE) - INTERVAL '1 day')) AS last_month_from_current, "
        "EXTRACT(day FROM (DATE_TRUNC('week', CURRENT_DATE) + INTERVAL '3 days')) AS wednesday_of_current_week, "
        "EXTRACT(quarter FROM (CURRENT_DATE + INTERVAL '3 months')) AS next_quarter_from_current "
        "FROM employees");
    
    // Test current_date() in WHERE clauses with all functions
    auto where_clause_tests = select(emp.id, emp.name)
        .from(emp)
        .where(
            // Multiple date functions with current_date in conditions
            (year(emp.hire_date) <= year(current_date())) &&
            (month(emp.birth_date) != month(current_date())) &&
            (day(emp.hire_date) >= day(current_date())) &&
            (date_diff("year", emp.birth_date, current_date()) >= 18) &&
            (date_add(emp.hire_date, interval("1 year")) < current_date()) &&
            (date_sub(current_date(), interval("5 years")) > start_of_year(emp.birth_date)) &&
            (extract("dow", current_date()) != extract("dow", emp.hire_date)) &&
            (date_trunc("month", current_date()) >= date_trunc("month", emp.last_review))
        );
    
    EXPECT_EQ(where_clause_tests.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE ((((((((EXTRACT(year FROM employees.hire_date) <= EXTRACT(year FROM CURRENT_DATE)) AND "
        "(EXTRACT(month FROM employees.birth_date) != EXTRACT(month FROM CURRENT_DATE))) AND "
        "(EXTRACT(day FROM employees.hire_date) >= EXTRACT(day FROM CURRENT_DATE))) AND "
        "(DATE_DIFF('year', employees.birth_date, CURRENT_DATE) >= ?)) AND "
        "((employees.hire_date + INTERVAL '1 year') < CURRENT_DATE)) AND "
        "((CURRENT_DATE - INTERVAL '5 years') > DATE_TRUNC('year', employees.birth_date))) AND "
        "(EXTRACT(dow FROM CURRENT_DATE) != EXTRACT(dow FROM employees.hire_date))) AND "
        "(DATE_TRUNC('month', CURRENT_DATE) >= DATE_TRUNC('month', employees.last_review)))");
    
    auto params = where_clause_tests.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
    
    // Test current_date() in ORDER BY clauses with all functions
    auto order_by_tests = select(emp.id, emp.name)
        .from(emp)
        .order_by(
            desc(date_diff("day", emp.hire_date, current_date())),
            asc(abs(date_diff("month", emp.birth_date, current_date()))),
            desc(year(current_date()) - year(emp.hire_date)),
            asc(month(date_add(current_date(), interval("6 months"))))
        );
    
    EXPECT_EQ(order_by_tests.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "ORDER BY DATE_DIFF('day', employees.hire_date, CURRENT_DATE) DESC, "
        "ABS(DATE_DIFF('month', employees.birth_date, CURRENT_DATE)) ASC, "
        "(EXTRACT(year FROM CURRENT_DATE) - EXTRACT(year FROM employees.hire_date)) DESC, "
        "EXTRACT(month FROM (CURRENT_DATE + INTERVAL '6 months')) ASC");
    
    // Test current_date() in GROUP BY with aggregate functions
    auto group_by_tests = select_expr(
        year(current_date()),
        month(current_date()),
        as(count_all(), "total_employees"),
        as(avg(date_diff("year", emp.birth_date, current_date())), "avg_age"),
        as(min(date_diff("day", emp.hire_date, current_date())), "min_tenure_days"),
        as(max(date_diff("day", emp.hire_date, current_date())), "max_tenure_days")
    )
    .from(emp)
    .group_by(
        year(current_date()),
        month(current_date())
    )
    .having(
        count_all() > 0 &&
        avg(date_diff("year", emp.birth_date, current_date())) >= 18
    );
    
    EXPECT_EQ(group_by_tests.to_sql(),
        "SELECT EXTRACT(year FROM CURRENT_DATE), EXTRACT(month FROM CURRENT_DATE), "
        "COUNT(*) AS total_employees, "
        "AVG(DATE_DIFF('year', employees.birth_date, CURRENT_DATE)) AS avg_age, "
        "MIN(DATE_DIFF('day', employees.hire_date, CURRENT_DATE)) AS min_tenure_days, "
        "MAX(DATE_DIFF('day', employees.hire_date, CURRENT_DATE)) AS max_tenure_days "
        "FROM employees "
        "GROUP BY EXTRACT(year FROM CURRENT_DATE), EXTRACT(month FROM CURRENT_DATE) "
        "HAVING ((COUNT(*) > ?) AND (AVG(DATE_DIFF('year', employees.birth_date, CURRENT_DATE)) >= ?))");
    
    params = group_by_tests.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "0");
    EXPECT_EQ(params[1], "18");
    
    // Test current_date() with comparison operators in all contexts
    auto comparison_tests = select(emp.id, emp.name)
        .from(emp)
        .where(
            (current_date() > emp.hire_date) &&
            (current_date() >= start_of_year(emp.birth_date)) &&
            (current_date() < (emp.hire_date + interval("50 years"))) &&
            (current_date() <= date_add(emp.birth_date, interval("100 years"))) &&
            (current_date() == date_trunc("day", current_date())) &&
            (current_date() != emp.termination_date)
        );
    
    EXPECT_EQ(comparison_tests.to_sql(),
        "SELECT employees.id, employees.name FROM employees "
        "WHERE ((((((CURRENT_DATE > employees.hire_date) AND "
        "(CURRENT_DATE >= DATE_TRUNC('year', employees.birth_date))) AND "
        "(CURRENT_DATE < (employees.hire_date + INTERVAL '50 years'))) AND "
        "(CURRENT_DATE <= (employees.birth_date + INTERVAL '100 years'))) AND "
        "(CURRENT_DATE = DATE_TRUNC('day', CURRENT_DATE))) AND "
        "(CURRENT_DATE != employees.termination_date))");
    
    // Verify all queries have valid SQL and no unexpected parameters
    EXPECT_TRUE(date_diff_tests.bind_params().empty());
    EXPECT_TRUE(date_arithmetic_tests.bind_params().empty());
    EXPECT_TRUE(extract_tests.bind_params().empty());
    EXPECT_TRUE(date_trunc_tests.bind_params().empty());
    EXPECT_TRUE(extract_helper_tests.bind_params().empty());
    EXPECT_TRUE(trunc_helper_tests.bind_params().empty());
    EXPECT_TRUE(helper_diff_tests.bind_params().empty());
    EXPECT_TRUE(complex_nested_current.bind_params().empty());
    EXPECT_TRUE(order_by_tests.bind_params().empty());
    EXPECT_TRUE(comparison_tests.bind_params().empty());
} 