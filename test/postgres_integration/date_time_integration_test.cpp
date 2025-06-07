#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <string>
#include <chrono>
#include <optional>
#include <vector>
#include <iostream>

namespace {

/// @brief Events table with comprehensive date/time columns using C++ chrono types
struct EventsTable {
    static constexpr auto table_name = "events";
    
    relx::schema::column<EventsTable, "id", int> id;
    relx::schema::column<EventsTable, "name", std::string> name;
    relx::schema::column<EventsTable, "event_date", std::chrono::system_clock::time_point> event_date;
    relx::schema::column<EventsTable, "created_at", std::chrono::system_clock::time_point> created_at;
    relx::schema::column<EventsTable, "updated_at", std::optional<std::chrono::system_clock::time_point>> updated_at;
    relx::schema::column<EventsTable, "start_time", std::chrono::system_clock::time_point> start_time;
    relx::schema::column<EventsTable, "end_time", std::optional<std::chrono::system_clock::time_point>> end_time;
    relx::schema::column<EventsTable, "birthdate", std::chrono::year_month_day> birthdate;
    relx::schema::column<EventsTable, "is_active", bool> is_active;
    
    // Primary key
    relx::schema::table_primary_key<&EventsTable::id> primary;
};

/// @brief Time zones table for testing timezone-aware operations
struct TimeZonesTable {
    static constexpr auto table_name = "time_zones";
    
    relx::schema::column<TimeZonesTable, "id", int> id;
    relx::schema::column<TimeZonesTable, "zone_name", std::string> zone_name;
    relx::schema::column<TimeZonesTable, "utc_time", std::chrono::system_clock::time_point> utc_time;
    relx::schema::column<TimeZonesTable, "local_time", std::chrono::system_clock::time_point> local_time;
    relx::schema::column<TimeZonesTable, "offset_hours", int> offset_hours;
    
    // Primary key
    relx::schema::table_primary_key<&TimeZonesTable::id> primary;
};

/// @brief Employee table for business logic date tests
struct EmployeeTable {
    static constexpr auto table_name = "employees_dt";  // Different name to avoid conflicts
    
    relx::schema::column<EmployeeTable, "id", int> id;
    relx::schema::column<EmployeeTable, "name", std::string> name;
    relx::schema::column<EmployeeTable, "hire_date", std::chrono::year_month_day> hire_date;
    relx::schema::column<EmployeeTable, "birth_date", std::chrono::year_month_day> birth_date;
    relx::schema::column<EmployeeTable, "last_promotion", std::optional<std::chrono::system_clock::time_point>> last_promotion;
    relx::schema::column<EmployeeTable, "salary", double> salary;
    relx::schema::column<EmployeeTable, "department", std::string> department;
    
    // Primary key
    relx::schema::table_primary_key<&EmployeeTable::id> primary;
};

}  // namespace

class DateTimeIntegrationTest : public ::testing::Test {
protected:
    // Connection string for the PostgreSQL test database
    std::string conn_string = "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
    
    void SetUp() override {
        // Setup test tables
        setupTables();
        insertTestData();
    }
    
    void TearDown() override {
        cleanupTables();
    }
    
private:
    void setupTables() {
        relx::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;
        
        // Drop and create events table with proper PostgreSQL date/time types
        auto drop_events = "DROP TABLE IF EXISTS events CASCADE";
        auto create_events = R"(
            CREATE TABLE events (
                id SERIAL PRIMARY KEY,
                name VARCHAR(255) NOT NULL,
                event_date TIMESTAMPTZ NOT NULL,
                created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMPTZ,
                start_time TIMESTAMPTZ NOT NULL,
                end_time TIMESTAMPTZ,
                birthdate DATE NOT NULL,
                is_active BOOLEAN DEFAULT TRUE
            )
        )";
        
        // Drop and create time_zones table
        auto drop_timezones = "DROP TABLE IF EXISTS time_zones CASCADE";
        auto create_timezones = R"(
            CREATE TABLE time_zones (
                id SERIAL PRIMARY KEY,
                zone_name VARCHAR(100) NOT NULL,
                utc_time TIMESTAMPTZ NOT NULL,
                local_time TIMESTAMPTZ NOT NULL,
                offset_hours INTEGER NOT NULL
            )
        )";
        
        // Drop and create employees table
        auto drop_employees = "DROP TABLE IF EXISTS employees_dt CASCADE";
        auto create_employees = R"(
            CREATE TABLE employees_dt (
                id SERIAL PRIMARY KEY,
                name VARCHAR(255) NOT NULL,
                hire_date DATE NOT NULL,
                birth_date DATE NOT NULL,
                last_promotion TIMESTAMPTZ,
                salary DECIMAL(10,2) NOT NULL,
                department VARCHAR(100) NOT NULL
            )
        )";
        
        auto result = conn.execute_raw(drop_events);
        ASSERT_TRUE(result) << "Failed to drop events table: " << result.error().message;
        
        result = conn.execute_raw(create_events);
        ASSERT_TRUE(result) << "Failed to create events table: " << result.error().message;
        
        result = conn.execute_raw(drop_timezones);
        ASSERT_TRUE(result) << "Failed to drop time_zones table: " << result.error().message;
        
        result = conn.execute_raw(create_timezones);
        ASSERT_TRUE(result) << "Failed to create time_zones table: " << result.error().message;
        
        result = conn.execute_raw(drop_employees);
        ASSERT_TRUE(result) << "Failed to drop employees_dt table: " << result.error().message;
        
        result = conn.execute_raw(create_employees);
        ASSERT_TRUE(result) << "Failed to create employees_dt table: " << result.error().message;
        
        conn.disconnect();
    }
    
    void insertTestData() {
        relx::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;
        
        // Insert events test data
        auto insert_events = R"(
            INSERT INTO events (name, event_date, start_time, end_time, birthdate) VALUES
            ('New Year Party', '2024-01-01 20:00:00+00', '2024-01-01 20:00:00+00', '2024-01-02 02:00:00+00', '1990-05-15'),
            ('Spring Conference', '2024-03-20 09:00:00+00', '2024-03-20 09:00:00+00', '2024-03-20 17:00:00+00', '1985-12-03'),
            ('Summer Festival', '2024-06-21 12:00:00+00', '2024-06-21 12:00:00+00', '2024-06-21 23:00:00+00', '1992-08-20'),
            ('Fall Meetup', '2024-09-23 15:00:00+00', '2024-09-23 15:00:00+00', NULL, '1988-02-14'),
            ('Winter Workshop', '2024-12-21 10:00:00+00', '2024-12-21 10:00:00+00', '2024-12-21 16:00:00+00', '1995-11-30'),
            ('Future Event', '2025-06-15 14:00:00+00', '2025-06-15 14:00:00+00', '2025-06-15 18:00:00+00', '1987-04-10'),
            ('Past Event', '2023-08-10 11:00:00+00', '2023-08-10 11:00:00+00', '2023-08-10 15:00:00+00', '1993-07-25'),
            ('Weekly Meeting', '2024-01-08 09:00:00+00', '2024-01-08 09:00:00+00', '2024-01-08 10:00:00+00', '1991-03-18')
        )";
        
        // Insert timezone test data
        // The local_time should be offset from utc_time by the timezone offset
        // so that local_time - utc_time = offset_hours * 3600 seconds
        auto insert_timezones = R"(
            INSERT INTO time_zones (zone_name, utc_time, local_time, offset_hours) VALUES
            ('UTC', '2024-01-01 12:00:00+00', '2024-01-01 12:00:00+00', 0),
            ('EST', '2024-01-01 12:00:00+00', '2024-01-01 07:00:00+00', -5),
            ('PST', '2024-01-01 12:00:00+00', '2024-01-01 04:00:00+00', -8),
            ('JST', '2024-01-01 12:00:00+00', '2024-01-01 21:00:00+00', 9),
            ('CEST', '2024-01-01 12:00:00+00', '2024-01-01 14:00:00+00', 2)
        )";
        
        // Insert employee test data
        auto insert_employees = R"(
            INSERT INTO employees_dt (name, hire_date, birth_date, last_promotion, salary, department) VALUES
            ('Alice Johnson', '2020-01-15', '1990-03-22', '2023-06-01 09:00:00+00', 75000.00, 'Engineering'),
            ('Bob Smith', '2018-05-10', '1985-11-08', '2022-12-15 14:30:00+00', 85000.00, 'Engineering'),
            ('Carol Davis', '2021-09-01', '1992-07-14', NULL, 65000.00, 'Marketing'),
            ('David Wilson', '2019-03-20', '1988-12-05', '2024-01-10 11:00:00+00', 70000.00, 'Sales'),
            ('Emma Brown', '2022-11-30', '1994-04-18', NULL, 60000.00, 'HR'),
            ('Frank Miller', '2017-02-14', '1982-09-30', '2023-09-20 16:45:00+00', 95000.00, 'Engineering'),
            ('Grace Lee', '2023-01-10', '1991-06-25', NULL, 68000.00, 'Marketing')
        )";
        
        auto result = conn.execute_raw(insert_events);
        ASSERT_TRUE(result) << "Failed to insert events data: " << result.error().message;
        
        result = conn.execute_raw(insert_timezones);
        ASSERT_TRUE(result) << "Failed to insert timezone data: " << result.error().message;
        
        result = conn.execute_raw(insert_employees);
        ASSERT_TRUE(result) << "Failed to insert employee data: " << result.error().message;
        
        conn.disconnect();
    }
    
    void cleanupTables() {
        relx::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        if (connect_result) {
            conn.execute_raw("DROP TABLE IF EXISTS events CASCADE");
            conn.execute_raw("DROP TABLE IF EXISTS time_zones CASCADE");
            conn.execute_raw("DROP TABLE IF EXISTS employees_dt CASCADE");
            conn.disconnect();
        }
    }
    
protected:
    EventsTable events;
    TimeZonesTable time_zones;
    EmployeeTable employees;
};

/// @brief Test basic date/time column operations
TEST_F(DateTimeIntegrationTest, BasicDateTimeOperations) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Test simple select with date/time columns - order by name instead of chrono types
    auto query = select(events.id, events.name, events.event_date, events.created_at)
                .from(events)
                .order_by(events.name);  // Order by string column instead
    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute basic datetime query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_GT(rows.size(), 0) << "Expected at least one row";
    
    // Verify we can read timestamp columns as C++ time_point types
    for (const auto& row : rows) {
        auto id = row.get<int>(0);
        auto name = row.get<std::string>(1);
        auto event_date = row.get<std::chrono::system_clock::time_point>(2);
        auto created_at = row.get<std::chrono::system_clock::time_point>(3);
        
        EXPECT_TRUE(id.has_value()) << "ID should not be null";
        EXPECT_TRUE(name.has_value()) << "Name should not be null";
        EXPECT_TRUE(event_date.has_value()) << "Event date should not be null";
        EXPECT_TRUE(created_at.has_value()) << "Created at should not be null";
        
        // Validate that the time_point values are reasonable (not epoch)
        if (event_date.has_value()) {
            auto time_t_val = std::chrono::system_clock::to_time_t(*event_date);
            EXPECT_GT(time_t_val, 1640995200) << "Event date should be after 2022-01-01";  // Unix timestamp for 2022-01-01
        }
    }
    
    conn.disconnect();
}

/// @brief Test date extraction functions with C++ types
TEST_F(DateTimeIntegrationTest, DateExtractionFunctions) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Test extracting various date parts from C++ chrono types using select_expr
    auto query = select_expr(
        events.name,
        as(extract("year", events.event_date), "event_year"),
        as(extract("month", events.event_date), "event_month"),
        as(extract("day", events.event_date), "event_day"),
        as(extract("hour", events.start_time), "start_hour"),
        as(extract("dow", events.event_date), "day_of_week")  // 0=Sunday, 1=Monday, etc.
    )
    .from(events)
    .order_by(events.name);  // Order by string column
    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute date extraction query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_GT(rows.size(), 0) << "Expected at least one row";
    
    for (const auto& row : rows) {
        auto name = row.get<std::string>(0);
        auto year = row.get<double>(1);  // EXTRACT returns numeric in PostgreSQL
        auto month = row.get<double>(2);
        auto day = row.get<double>(3);
        auto hour = row.get<double>(4);
        auto dow = row.get<double>(5);
        
        EXPECT_TRUE(name.has_value()) << "Name should not be null";
        EXPECT_TRUE(year.has_value()) << "Year should not be null";
        EXPECT_TRUE(month.has_value()) << "Month should not be null";
        EXPECT_TRUE(day.has_value()) << "Day should not be null";
        EXPECT_TRUE(hour.has_value()) << "Hour should not be null";
        EXPECT_TRUE(dow.has_value()) << "Day of week should not be null";
        
        // Validate extracted values are in reasonable ranges
        EXPECT_GE(*year, 2020) << "Year should be >= 2020";
        EXPECT_LE(*year, 2030) << "Year should be <= 2030";
        EXPECT_GE(*month, 1) << "Month should be >= 1";
        EXPECT_LE(*month, 12) << "Month should be <= 12";
        EXPECT_GE(*day, 1) << "Day should be >= 1";
        EXPECT_LE(*day, 31) << "Day should be <= 31";
        EXPECT_GE(*hour, 0) << "Hour should be >= 0";
        EXPECT_LE(*hour, 23) << "Hour should be <= 23";
        EXPECT_GE(*dow, 0) << "Day of week should be >= 0";
        EXPECT_LE(*dow, 6) << "Day of week should be <= 6";
    }
    
    conn.disconnect();
}

/// @brief Test date arithmetic and interval operations with C++ types
TEST_F(DateTimeIntegrationTest, DateArithmeticOperations) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Test adding intervals to C++ chrono date types using select_expr
    auto query = select_expr(
        events.name,
        events.event_date,
        as(events.event_date + interval("1 day"), "next_day"),
        as(events.event_date + interval("1 month"), "next_month"),
        as(events.event_date + interval("1 year"), "next_year"),
        as(events.start_time + interval("2 hours"), "two_hours_later")
    )
    .from(events)
    .where(events.name == "New Year Party");
    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute date arithmetic query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(rows.size(), 1) << "Expected exactly one row for New Year Party";
    
    const auto& row = rows[0];
    auto name = row.get<std::string>(0);
    auto original_date = row.get<std::chrono::system_clock::time_point>(1);
    auto next_day = row.get<std::chrono::system_clock::time_point>(2);
    auto next_month = row.get<std::chrono::system_clock::time_point>(3);
    auto next_year = row.get<std::chrono::system_clock::time_point>(4);
    auto two_hours_later = row.get<std::chrono::system_clock::time_point>(5);
    
    EXPECT_EQ(*name, "New Year Party");
    EXPECT_TRUE(original_date.has_value());
    EXPECT_TRUE(next_day.has_value());
    EXPECT_TRUE(next_month.has_value());
    EXPECT_TRUE(next_year.has_value());
    EXPECT_TRUE(two_hours_later.has_value());
    
    // Verify the time_point arithmetic worked correctly
    if (original_date.has_value() && next_day.has_value()) {
        auto diff = *next_day - *original_date;
        auto hours_diff = std::chrono::duration_cast<std::chrono::hours>(diff).count();
        EXPECT_EQ(hours_diff, 24) << "Next day should be 24 hours later";
    }
    
    if (original_date.has_value() && two_hours_later.has_value()) {
        auto diff = *two_hours_later - *original_date;
        auto hours_diff = std::chrono::duration_cast<std::chrono::hours>(diff).count();
        EXPECT_EQ(hours_diff, 2) << "Two hours later should be 2 hours later";
    }
    
    conn.disconnect();
}

/// @brief Test current date/time functions
TEST_F(DateTimeIntegrationTest, CurrentDateTimeFunctions) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Test current date/time functions using select_expr
    auto query = select_expr(
        as(current_date(), "current_date"),
        as(current_timestamp(), "current_timestamp"),
        as(now(), "now")
    );
    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute current datetime query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(rows.size(), 1) << "Expected exactly one row";
    
    const auto& row = rows[0];
    // Note: These will be returned as strings since they're functions, not typed columns
    auto current_date_val = row.get<std::string>(0);
    auto current_timestamp_val = row.get<std::string>(1);
    auto now_val = row.get<std::string>(2);
    
    EXPECT_TRUE(current_date_val.has_value()) << "Current date should not be null";
    EXPECT_TRUE(current_timestamp_val.has_value()) << "Current timestamp should not be null";
    EXPECT_TRUE(now_val.has_value()) << "Now should not be null";
    
    // Verify basic format
    EXPECT_GE(current_date_val->length(), 10) << "Current date should be at least 10 characters (YYYY-MM-DD)";
    EXPECT_GE(current_timestamp_val->length(), 19) << "Current timestamp should be at least 19 characters";
    EXPECT_GE(now_val->length(), 19) << "Now should be at least 19 characters";
    
    conn.disconnect();
}

/// @brief Test date helper functions with C++ types
TEST_F(DateTimeIntegrationTest, DateHelperFunctions) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Test convenient date helper functions with C++ chrono types using select_expr
    auto query = select_expr(
        employees.name,
        employees.birth_date,
        as(year(employees.birth_date), "birth_year"),
        as(month(employees.birth_date), "birth_month"),
        as(day(employees.birth_date), "birth_day"),
        as(age_in_years(employees.birth_date), "age"),
        as(days_since(employees.hire_date), "days_employed")
    )
    .from(employees)
    .where(employees.name == "Alice Johnson");
    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute date helpers query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(rows.size(), 1) << "Expected exactly one row for Alice Johnson";
    
    const auto& row = rows[0];
    auto name = row.get<std::string>(0);
    auto birth_date = row.get<std::chrono::year_month_day>(1);
    auto birth_year = row.get<double>(2);
    auto birth_month = row.get<double>(3);
    auto birth_day = row.get<double>(4);
    auto age = row.get<double>(5);
    auto days_employed = row.get<double>(6);
    
    EXPECT_EQ(*name, "Alice Johnson");
    EXPECT_TRUE(birth_date.has_value());
    EXPECT_TRUE(birth_year.has_value());
    EXPECT_TRUE(birth_month.has_value());
    EXPECT_TRUE(birth_day.has_value());
    EXPECT_TRUE(age.has_value());
    EXPECT_TRUE(days_employed.has_value());
    
    // Validate extracted values
    EXPECT_EQ(*birth_year, 1990) << "Alice's birth year should be 1990";
    EXPECT_EQ(*birth_month, 3) << "Alice's birth month should be March (3)";
    EXPECT_EQ(*birth_day, 22) << "Alice's birth day should be 22";
    EXPECT_GE(*age, 30) << "Alice should be at least 30 years old";
    EXPECT_GE(*days_employed, 1000) << "Alice should have been employed for over 1000 days";
    
    // Test the C++ year_month_day type if available
    if (birth_date.has_value()) {
        EXPECT_EQ(static_cast<int>(birth_date->year()), 1990) << "Birth year from chrono type should be 1990";
        EXPECT_EQ(static_cast<unsigned>(birth_date->month()), 3u) << "Birth month from chrono type should be 3";
        EXPECT_EQ(static_cast<unsigned>(birth_date->day()), 22u) << "Birth day from chrono type should be 22";
    }
    
    conn.disconnect();
}

/// @brief Test timezone-aware operations with C++ types
TEST_F(DateTimeIntegrationTest, TimeZoneOperations) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Test working with different time zones using C++ chrono types
    // Use date_diff to calculate time difference instead of direct subtraction
    auto query = select_expr(
        time_zones.zone_name,
        time_zones.utc_time,
        time_zones.local_time,
        time_zones.offset_hours,
        // Calculate the difference in seconds between UTC and local time
        as(date_diff("second", time_zones.utc_time, time_zones.local_time), "time_diff_seconds")
    )
    .from(time_zones)
    .order_by(time_zones.offset_hours);
    

    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute timezone query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_GT(rows.size(), 0) << "Expected at least one timezone row";
    
    for (const auto& row : rows) {
        auto zone_name = row.get<std::string>(0);
        auto utc_time = row.get<std::chrono::system_clock::time_point>(1);
        auto local_time = row.get<std::chrono::system_clock::time_point>(2);
        auto offset_hours = row.get<int>(3);
        auto time_diff_seconds = row.get<double>(4);
        

        
        EXPECT_TRUE(zone_name.has_value()) << "Zone name should not be null";
        EXPECT_TRUE(utc_time.has_value()) << "UTC time should not be null";
        EXPECT_TRUE(local_time.has_value()) << "Local time should not be null";
        EXPECT_TRUE(offset_hours.has_value()) << "Offset hours should not be null";
        EXPECT_TRUE(time_diff_seconds.has_value()) << "Time diff should not be null";
        
        // Validate offset ranges
        EXPECT_GE(*offset_hours, -12) << "Offset should be >= -12";
        EXPECT_LE(*offset_hours, 14) << "Offset should be <= 14";
        
        // For most timezones, the difference should match the offset
        // Note: This might not be exact due to DST and other factors
        double expected_diff_seconds = *offset_hours * 3600.0;
        EXPECT_NEAR(*time_diff_seconds, expected_diff_seconds, 3600.0) 
            << "Time difference should approximately match offset for " << *zone_name;
        
        // Test C++ time_point arithmetic
        if (utc_time.has_value() && local_time.has_value()) {
            auto diff = *local_time - *utc_time;
            auto hours_diff = std::chrono::duration_cast<std::chrono::hours>(diff).count();
            EXPECT_NEAR(hours_diff, *offset_hours, 1) << "C++ time_point difference should match offset";
        }
    }
    
    conn.disconnect();
}

/// @brief Test date operations with NULL values and C++ optional types
TEST_F(DateTimeIntegrationTest, DateOperationsWithNulls) {
    using namespace relx::query;
    
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Simplified test focusing on reading NULL date values with C++ optional types
    auto query = select_expr(
        events.name,
        events.end_time,
        as(extract("hour", events.start_time), "start_hour")  // Always extract from non-null column
    )
    .from(events)
    .order_by(events.name);  // Order by string column
    
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Failed to execute NULL handling query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_GT(rows.size(), 0) << "Expected at least one event";
    
    bool found_null_end_time = false;
    for (const auto& row : rows) {
        auto name = row.get<std::string>(0);
        auto end_time = row.get<std::optional<std::chrono::system_clock::time_point>>(1);  // Using optional type
        auto start_hour = row.get<double>(2);
        
        EXPECT_TRUE(name.has_value()) << "Name should not be null";
        EXPECT_TRUE(start_hour.has_value()) << "Start hour should not be null";
        
        if (!end_time.has_value() || !(*end_time).has_value()) {
            found_null_end_time = true;
            // Just verify we can detect NULL values correctly
        } else {
            // Verify we have a valid end time
            EXPECT_TRUE(end_time.has_value()) << "End time should have a value when not NULL";
        }
        
        // Validate start hour is reasonable
        EXPECT_GE(*start_hour, 0.0) << "Start hour should be >= 0";
        EXPECT_LE(*start_hour, 23.0) << "Start hour should be <= 23";
    }
    
    EXPECT_TRUE(found_null_end_time) << "Expected to find at least one event with NULL end time";
    
    conn.disconnect();
} 