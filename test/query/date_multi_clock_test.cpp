#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>
#include <chrono>
#include <optional>

using namespace relx;
using namespace relx::query;

// Test table using system_clock
struct SystemClockTable {
    static constexpr auto table_name = "system_clock_events";
    
    schema::column<SystemClockTable, "id", int> id;
    schema::column<SystemClockTable, "name", std::string> name;
    schema::column<SystemClockTable, "timestamp", std::chrono::system_clock::time_point> timestamp;
    schema::column<SystemClockTable, "optional_timestamp", std::optional<std::chrono::system_clock::time_point>> optional_timestamp;
    
    schema::pk<&SystemClockTable::id> primary;
};

// Test table using steady_clock
struct SteadyClockTable {
    static constexpr auto table_name = "steady_clock_events";
    
    schema::column<SteadyClockTable, "id", int> id;
    schema::column<SteadyClockTable, "name", std::string> name;
    schema::column<SteadyClockTable, "timestamp", std::chrono::steady_clock::time_point> timestamp;
    schema::column<SteadyClockTable, "optional_timestamp", std::optional<std::chrono::steady_clock::time_point>> optional_timestamp;
    
    schema::pk<&SteadyClockTable::id> primary;
};

// Test table using high_resolution_clock
struct HighResClockTable {
    static constexpr auto table_name = "high_res_clock_events";
    
    schema::column<HighResClockTable, "id", int> id;
    schema::column<HighResClockTable, "name", std::string> name;
    schema::column<HighResClockTable, "timestamp", std::chrono::high_resolution_clock::time_point> timestamp;
    schema::column<HighResClockTable, "optional_timestamp", std::optional<std::chrono::high_resolution_clock::time_point>> optional_timestamp;
    
    schema::pk<&HighResClockTable::id> primary;
};

// Test table mixing different clock types
struct MixedClockTable {
    static constexpr auto table_name = "mixed_clock_events";
    
    schema::column<MixedClockTable, "id", int> id;
    schema::column<MixedClockTable, "system_time", std::chrono::system_clock::time_point> system_time;
    schema::column<MixedClockTable, "steady_time", std::chrono::steady_clock::time_point> steady_time;
    schema::column<MixedClockTable, "high_res_time", std::chrono::high_resolution_clock::time_point> high_res_time;
    schema::column<MixedClockTable, "optional_system", std::optional<std::chrono::system_clock::time_point>> optional_system;
    schema::column<MixedClockTable, "optional_steady", std::optional<std::chrono::steady_clock::time_point>> optional_steady;
    schema::column<MixedClockTable, "optional_high_res", std::optional<std::chrono::high_resolution_clock::time_point>> optional_high_res;
    
    schema::pk<&MixedClockTable::id> primary;
};

class MultiClockDateTest : public ::testing::Test {
protected:
    SystemClockTable system_table;
    SteadyClockTable steady_table;
    HighResClockTable high_res_table;
    MixedClockTable mixed_table;
};

// Test 1: Basic date functions with system_clock (existing functionality)
TEST_F(MultiClockDateTest, SystemClockBasicFunctions) {
    // Test date_diff
    auto diff_query = select_expr(date_diff("day", system_table.timestamp, current_date()))
        .from(system_table);
    
    EXPECT_EQ(diff_query.to_sql(), "SELECT (CURRENT_DATE::date - system_clock_events.timestamp::date) FROM system_clock_events");
    EXPECT_TRUE(diff_query.bind_params().empty());
    
    // Test extract
    auto extract_query = select_expr(extract("year", system_table.timestamp))
        .from(system_table);
    
    EXPECT_EQ(extract_query.to_sql(), "SELECT EXTRACT(year FROM system_clock_events.timestamp) FROM system_clock_events");
    
    // Test date arithmetic
    auto arithmetic_query = select_expr(date_add(system_table.timestamp, interval("1 year")))
        .from(system_table);
    
    EXPECT_EQ(arithmetic_query.to_sql(), "SELECT (system_clock_events.timestamp + INTERVAL '1 year') FROM system_clock_events");
    
    // Test with optional column
    auto optional_query = select_expr(year(system_table.optional_timestamp))
        .from(system_table);
    
    EXPECT_EQ(optional_query.to_sql(), "SELECT EXTRACT(year FROM system_clock_events.optional_timestamp) FROM system_clock_events");
}

// Test 2: Basic date functions with steady_clock
TEST_F(MultiClockDateTest, SteadyClockBasicFunctions) {
    // Test date_diff
    auto diff_query = select_expr(date_diff("hour", steady_table.timestamp, now()))
        .from(steady_table);
    
    EXPECT_EQ(diff_query.to_sql(), "SELECT EXTRACT(EPOCH FROM (NOW() - steady_clock_events.timestamp))/3600 FROM steady_clock_events");
    EXPECT_TRUE(diff_query.bind_params().empty());
    
    // Test extract
    auto extract_query = select_expr(extract("month", steady_table.timestamp))
        .from(steady_table);
    
    EXPECT_EQ(extract_query.to_sql(), "SELECT EXTRACT(month FROM steady_clock_events.timestamp) FROM steady_clock_events");
    
    // Test date arithmetic with operator
    auto arithmetic_query = select_expr(steady_table.timestamp + interval("6 months"))
        .from(steady_table);
    
    EXPECT_EQ(arithmetic_query.to_sql(), "SELECT (steady_clock_events.timestamp + INTERVAL '6 months') FROM steady_clock_events");
    
    // Test with optional column
    auto optional_query = select_expr(month(steady_table.optional_timestamp))
        .from(steady_table);
    
    EXPECT_EQ(optional_query.to_sql(), "SELECT EXTRACT(month FROM steady_clock_events.optional_timestamp) FROM steady_clock_events");
}

// Test 3: Basic date functions with high_resolution_clock
TEST_F(MultiClockDateTest, HighResClockBasicFunctions) {
    // Test date_diff
    auto diff_query = select_expr(date_diff("minute", high_res_table.timestamp, current_timestamp()))
        .from(high_res_table);
    
    EXPECT_EQ(diff_query.to_sql(), "SELECT EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - high_res_clock_events.timestamp))/60 FROM high_res_clock_events");
    EXPECT_TRUE(diff_query.bind_params().empty());
    
    // Test extract
    auto extract_query = select_expr(extract("day", high_res_table.timestamp))
        .from(high_res_table);
    
    EXPECT_EQ(extract_query.to_sql(), "SELECT EXTRACT(day FROM high_res_clock_events.timestamp) FROM high_res_clock_events");
    
    // Test date truncation
    auto trunc_query = select_expr(date_trunc("hour", high_res_table.timestamp))
        .from(high_res_table);
    
    EXPECT_EQ(trunc_query.to_sql(), "SELECT DATE_TRUNC('hour', high_res_clock_events.timestamp) FROM high_res_clock_events");
    
    // Test with optional column
    auto optional_query = select_expr(day(high_res_table.optional_timestamp))
        .from(high_res_table);
    
    EXPECT_EQ(optional_query.to_sql(), "SELECT EXTRACT(day FROM high_res_clock_events.optional_timestamp) FROM high_res_clock_events");
}

// Test 4: Helper functions work with all clock types
TEST_F(MultiClockDateTest, HelperFunctionsAllClocks) {
    // Test helper functions with system_clock
    auto system_helpers = select_expr(
        as(year(system_table.timestamp), "sys_year"),
        as(month(system_table.timestamp), "sys_month"),
        as(day(system_table.timestamp), "sys_day"),
        as(hour(system_table.timestamp), "sys_hour"),
        as(minute(system_table.timestamp), "sys_minute"),
        as(second(system_table.timestamp), "sys_second")
    ).from(system_table);
    
    EXPECT_EQ(system_helpers.to_sql(),
        "SELECT "
        "EXTRACT(year FROM system_clock_events.timestamp) AS sys_year, "
        "EXTRACT(month FROM system_clock_events.timestamp) AS sys_month, "
        "EXTRACT(day FROM system_clock_events.timestamp) AS sys_day, "
        "EXTRACT(hour FROM system_clock_events.timestamp) AS sys_hour, "
        "EXTRACT(minute FROM system_clock_events.timestamp) AS sys_minute, "
        "EXTRACT(second FROM system_clock_events.timestamp) AS sys_second "
        "FROM system_clock_events");
    
    // Test helper functions with steady_clock
    auto steady_helpers = select_expr(
        as(year(steady_table.timestamp), "steady_year"),
        as(start_of_year(steady_table.timestamp), "steady_start_year"),
        as(start_of_month(steady_table.timestamp), "steady_start_month"),
        as(start_of_day(steady_table.timestamp), "steady_start_day")
    ).from(steady_table);
    
    EXPECT_EQ(steady_helpers.to_sql(),
        "SELECT "
        "EXTRACT(year FROM steady_clock_events.timestamp) AS steady_year, "
        "DATE_TRUNC('year', steady_clock_events.timestamp) AS steady_start_year, "
        "DATE_TRUNC('month', steady_clock_events.timestamp) AS steady_start_month, "
        "DATE_TRUNC('day', steady_clock_events.timestamp) AS steady_start_day "
        "FROM steady_clock_events");
    
    // Test helper functions with high_resolution_clock
    auto high_res_helpers = select_expr(
        as(day_of_week(high_res_table.timestamp), "hr_dow"),
        as(day_of_year(high_res_table.timestamp), "hr_doy"),
        as(hour(high_res_table.timestamp), "hr_hour"),
        as(minute(high_res_table.timestamp), "hr_minute"),
        as(second(high_res_table.timestamp), "hr_second")
    ).from(high_res_table);
    
    EXPECT_EQ(high_res_helpers.to_sql(),
        "SELECT "
        "EXTRACT(dow FROM high_res_clock_events.timestamp) AS hr_dow, "
        "EXTRACT(doy FROM high_res_clock_events.timestamp) AS hr_doy, "
        "EXTRACT(hour FROM high_res_clock_events.timestamp) AS hr_hour, "
        "EXTRACT(minute FROM high_res_clock_events.timestamp) AS hr_minute, "
        "EXTRACT(second FROM high_res_clock_events.timestamp) AS hr_second "
        "FROM high_res_clock_events");
}

// Test 5: Cross-clock operations (different clocks in same query)
TEST_F(MultiClockDateTest, MixedClockOperations) {
    // Select from table with multiple clock types
    auto mixed_query = select_expr(
        mixed_table.id,
        as(year(mixed_table.system_time), "sys_year"),
        as(month(mixed_table.steady_time), "steady_month"),
        as(day(mixed_table.high_res_time), "hr_day"),
        as(hour(mixed_table.optional_system), "opt_sys_hour"),
        as(minute(mixed_table.optional_steady), "opt_steady_minute"),
        as(second(mixed_table.optional_high_res), "opt_hr_second")
    ).from(mixed_table);
    
    EXPECT_EQ(mixed_query.to_sql(),
        "SELECT mixed_clock_events.id, "
        "EXTRACT(year FROM mixed_clock_events.system_time) AS sys_year, "
        "EXTRACT(month FROM mixed_clock_events.steady_time) AS steady_month, "
        "EXTRACT(day FROM mixed_clock_events.high_res_time) AS hr_day, "
        "EXTRACT(hour FROM mixed_clock_events.optional_system) AS opt_sys_hour, "
        "EXTRACT(minute FROM mixed_clock_events.optional_steady) AS opt_steady_minute, "
        "EXTRACT(second FROM mixed_clock_events.optional_high_res) AS opt_hr_second "
        "FROM mixed_clock_events");
    
    // Test operations mixing different clock types
    auto complex_mixed = select_expr(
        as(date_add(mixed_table.system_time, interval("1 year")), "sys_plus_year"),
        as(mixed_table.steady_time + interval("6 months"), "steady_plus_months"),
        as(date_sub(mixed_table.high_res_time, interval("1 week")), "hr_minus_week"),
        as(date_trunc("day", mixed_table.system_time), "sys_trunc_day"),
        as(start_of_month(mixed_table.steady_time), "steady_start_month"),
        as(extract("hour", mixed_table.high_res_time), "hr_extract_hour")
    ).from(mixed_table);
    
    EXPECT_EQ(complex_mixed.to_sql(),
        "SELECT "
        "(mixed_clock_events.system_time + INTERVAL '1 year') AS sys_plus_year, "
        "(mixed_clock_events.steady_time + INTERVAL '6 months') AS steady_plus_months, "
        "(mixed_clock_events.high_res_time - INTERVAL '1 week') AS hr_minus_week, "
        "DATE_TRUNC('day', mixed_clock_events.system_time) AS sys_trunc_day, "
        "DATE_TRUNC('month', mixed_clock_events.steady_time) AS steady_start_month, "
        "EXTRACT(hour FROM mixed_clock_events.high_res_time) AS hr_extract_hour "
        "FROM mixed_clock_events");
}

// Test 6: WHERE clauses with different clock types
TEST_F(MultiClockDateTest, WhereClausesWithDifferentClocks) {
    // Test WHERE with system_clock
    auto system_where = select(system_table.id, system_table.name)
        .from(system_table)
        .where(
            (year(system_table.timestamp) >= 2020) &&
            (month(system_table.timestamp) <= 6) &&
            (day(system_table.timestamp) > 15)
        );
    
    EXPECT_EQ(system_where.to_sql(),
        "SELECT system_clock_events.id, system_clock_events.name FROM system_clock_events "
        "WHERE (((EXTRACT(year FROM system_clock_events.timestamp) >= ?) AND "
        "(EXTRACT(month FROM system_clock_events.timestamp) <= ?)) AND "
        "(EXTRACT(day FROM system_clock_events.timestamp) > ?))");
    
    auto params = system_where.bind_params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "2020");
    EXPECT_EQ(params[1], "6");
    EXPECT_EQ(params[2], "15");
    
    // Test WHERE with steady_clock
    auto steady_where = select(steady_table.id, steady_table.name)
        .from(steady_table)
        .where(
            (hour(steady_table.timestamp) >= 9) &&
            (minute(steady_table.timestamp) < 30) &&
            (steady_table.optional_timestamp.is_not_null())
        );
    
    EXPECT_EQ(steady_where.to_sql(),
        "SELECT steady_clock_events.id, steady_clock_events.name FROM steady_clock_events "
        "WHERE (((EXTRACT(hour FROM steady_clock_events.timestamp) >= ?) AND "
        "(EXTRACT(minute FROM steady_clock_events.timestamp) < ?)) AND "
        "steady_clock_events.optional_timestamp IS NOT NULL)");
    
    params = steady_where.bind_params();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "9");
    EXPECT_EQ(params[1], "30");
    
    // Test WHERE with high_resolution_clock
    auto high_res_where = select(high_res_table.id, high_res_table.name)
        .from(high_res_table)
        .where(
            (second(high_res_table.timestamp) >= 0) &&
            (second(high_res_table.timestamp) < 30) &&
            (day_of_week(high_res_table.timestamp) != 0)  // Not Sunday
        );
    
    EXPECT_EQ(high_res_where.to_sql(),
        "SELECT high_res_clock_events.id, high_res_clock_events.name FROM high_res_clock_events "
        "WHERE (((EXTRACT(second FROM high_res_clock_events.timestamp) >= ?) AND "
        "(EXTRACT(second FROM high_res_clock_events.timestamp) < ?)) AND "
        "(EXTRACT(dow FROM high_res_clock_events.timestamp) != ?))");
    
    params = high_res_where.bind_params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "0");
    EXPECT_EQ(params[1], "30");
    EXPECT_EQ(params[2], "0");
}

// Test 7: ORDER BY clauses with different clock types
TEST_F(MultiClockDateTest, OrderByWithDifferentClocks) {
    // Test ORDER BY with mixed clock types
    auto mixed_order = select(mixed_table.id)
        .from(mixed_table)
        .order_by(
            desc(year(mixed_table.system_time)),
            asc(month(mixed_table.steady_time)),
            desc(day(mixed_table.high_res_time)),
            asc(hour(mixed_table.optional_system))
        );
    
    EXPECT_EQ(mixed_order.to_sql(),
        "SELECT mixed_clock_events.id FROM mixed_clock_events "
        "ORDER BY EXTRACT(year FROM mixed_clock_events.system_time) DESC, "
        "EXTRACT(month FROM mixed_clock_events.steady_time) ASC, "
        "EXTRACT(day FROM mixed_clock_events.high_res_time) DESC, "
        "EXTRACT(hour FROM mixed_clock_events.optional_system) ASC");
}

// Test 8: GROUP BY with aggregates using different clock types
TEST_F(MultiClockDateTest, GroupByWithDifferentClocks) {
    // Test GROUP BY with different clock types
    auto grouped_query = select_expr(
        year(mixed_table.system_time),
        month(mixed_table.steady_time),
        as(count_all(), "event_count"),
        as(min(day(mixed_table.high_res_time)), "min_day"),
        as(max(hour(mixed_table.optional_system)), "max_hour")
    )
    .from(mixed_table)
    .group_by(
        year(mixed_table.system_time),
        month(mixed_table.steady_time)
    )
    .having(
        count_all() > 5
    );
    
    EXPECT_EQ(grouped_query.to_sql(),
        "SELECT EXTRACT(year FROM mixed_clock_events.system_time), "
        "EXTRACT(month FROM mixed_clock_events.steady_time), "
        "COUNT(*) AS event_count, "
        "MIN(EXTRACT(day FROM mixed_clock_events.high_res_time)) AS min_day, "
        "MAX(EXTRACT(hour FROM mixed_clock_events.optional_system)) AS max_hour "
        "FROM mixed_clock_events "
        "GROUP BY EXTRACT(year FROM mixed_clock_events.system_time), "
        "EXTRACT(month FROM mixed_clock_events.steady_time) "
        "HAVING (COUNT(*) > ?)");
    
    auto params = grouped_query.bind_params();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "5");
}

// Test 9: Complex nested operations with multiple clock types
TEST_F(MultiClockDateTest, ComplexNestedMultiClock) {
    // Test complex nested date functions with different clock types
    auto complex_query = select_expr(
        mixed_table.id,
        as(
            year(date_add(start_of_year(mixed_table.system_time), interval("6 months"))),
            "mid_year_sys"
        ),
        as(
            month(mixed_table.steady_time + interval("3 months")),
            "future_month_steady"
        ),
        as(
            day(date_trunc("month", mixed_table.high_res_time)),
            "month_start_day_hr"
        ),
        as(
            hour(date_sub(mixed_table.optional_system, interval("2 hours"))),
            "past_hour_opt_sys"
        )
    )
    .from(mixed_table)
    .where(
        (year(mixed_table.system_time) >= year(mixed_table.steady_time)) &&
        (month(mixed_table.steady_time) == month(mixed_table.high_res_time)) &&
        (day(mixed_table.system_time) <= day(mixed_table.high_res_time))
    );
    
    EXPECT_EQ(complex_query.to_sql(),
        "SELECT mixed_clock_events.id, "
        "EXTRACT(year FROM (DATE_TRUNC('year', mixed_clock_events.system_time) + INTERVAL '6 months')) AS mid_year_sys, "
        "EXTRACT(month FROM (mixed_clock_events.steady_time + INTERVAL '3 months')) AS future_month_steady, "
        "EXTRACT(day FROM DATE_TRUNC('month', mixed_clock_events.high_res_time)) AS month_start_day_hr, "
        "EXTRACT(hour FROM (mixed_clock_events.optional_system - INTERVAL '2 hours')) AS past_hour_opt_sys "
        "FROM mixed_clock_events "
        "WHERE (((EXTRACT(year FROM mixed_clock_events.system_time) >= EXTRACT(year FROM mixed_clock_events.steady_time)) AND "
        "(EXTRACT(month FROM mixed_clock_events.steady_time) = EXTRACT(month FROM mixed_clock_events.high_res_time))) AND "
        "(EXTRACT(day FROM mixed_clock_events.system_time) <= EXTRACT(day FROM mixed_clock_events.high_res_time)))");
}

// Test 10: Verify type safety - ensure compilation succeeds for valid clock types
TEST_F(MultiClockDateTest, TypeSafetyCompilation) {
    // These should all compile successfully - valid time_point operations
    auto valid_system = select_expr(
        date_diff("day", system_table.timestamp, current_date()),
        extract("year", system_table.timestamp),
        date_add(system_table.timestamp, interval("1 year")),
        year(system_table.timestamp),
        system_table.timestamp + interval("6 months")
    ).from(system_table);
    
    auto valid_steady = select_expr(
        date_diff("hour", steady_table.timestamp, now()),
        extract("month", steady_table.timestamp),
        date_sub(steady_table.timestamp, interval("2 weeks")),
        month(steady_table.timestamp),
        steady_table.timestamp - interval("1 day")
    ).from(steady_table);
    
    auto valid_high_res = select_expr(
        date_diff("minute", high_res_table.timestamp, current_timestamp()),
        extract("day", high_res_table.timestamp),
        date_trunc("hour", high_res_table.timestamp),
        day(high_res_table.timestamp),
        high_res_table.timestamp + interval("30 minutes")
    ).from(high_res_table);
    
    // Test that all queries generate valid SQL
    EXPECT_FALSE(valid_system.to_sql().empty());
    EXPECT_FALSE(valid_steady.to_sql().empty());
    EXPECT_FALSE(valid_high_res.to_sql().empty());
    
    // Test optional columns work too
    auto valid_optional = select_expr(
        year(system_table.optional_timestamp),
        month(steady_table.optional_timestamp),
        day(high_res_table.optional_timestamp)
    ).from(system_table);
    
    EXPECT_FALSE(valid_optional.to_sql().empty());
    
    EXPECT_TRUE(true); // Test that all valid cases compile
}

// Test 11: Special helper functions with different clocks  
TEST_F(MultiClockDateTest, SpecialHelpersWithDifferentClocks) {
    // Note: age_in_years, days_since, days_until work with current_date() 
    // which is system_clock based, so they're most meaningful with system_clock
    // but should still work syntactically with other clocks
    
    // Test with system_clock (most realistic use case)
    auto system_helpers = select_expr(
        as(age_in_years(system_table.timestamp), "age_years"),
        as(days_since(system_table.timestamp), "days_since"),
        as(days_until(system_table.timestamp), "days_until")
    ).from(system_table);
    
    EXPECT_EQ(system_helpers.to_sql(),
        "SELECT "
        "EXTRACT(YEAR FROM AGE(CURRENT_DATE, system_clock_events.timestamp)) AS age_years, "
        "(CURRENT_DATE::date - system_clock_events.timestamp::date) AS days_since, "
        "(system_clock_events.timestamp::date - CURRENT_DATE::date) AS days_until "
        "FROM system_clock_events");
    
    // Test with steady_clock (syntactically valid, but semantically these helpers
    // are less meaningful since steady_clock doesn't relate to calendar time)
    auto steady_helpers = select_expr(
        as(age_in_years(steady_table.timestamp), "steady_age"),
        as(days_since(steady_table.timestamp), "steady_days_since"),
        as(days_until(steady_table.timestamp), "steady_days_until")
    ).from(steady_table);
    
    EXPECT_EQ(steady_helpers.to_sql(),
        "SELECT "
        "EXTRACT(YEAR FROM AGE(CURRENT_DATE, steady_clock_events.timestamp)) AS steady_age, "
        "(CURRENT_DATE::date - steady_clock_events.timestamp::date) AS steady_days_since, "
        "(steady_clock_events.timestamp::date - CURRENT_DATE::date) AS steady_days_until "
        "FROM steady_clock_events");
    
    // Test with high_resolution_clock
    auto high_res_helpers = select_expr(
        as(age_in_years(high_res_table.timestamp), "hr_age"),
        as(days_since(high_res_table.timestamp), "hr_days_since"),
        as(days_until(high_res_table.timestamp), "hr_days_until")
    ).from(high_res_table);
    
    EXPECT_EQ(high_res_helpers.to_sql(),
        "SELECT "
        "EXTRACT(YEAR FROM AGE(CURRENT_DATE, high_res_clock_events.timestamp)) AS hr_age, "
        "(CURRENT_DATE::date - high_res_clock_events.timestamp::date) AS hr_days_since, "
        "(high_res_clock_events.timestamp::date - CURRENT_DATE::date) AS hr_days_until "
        "FROM high_res_clock_events");
} 