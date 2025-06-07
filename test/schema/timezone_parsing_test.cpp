#include "relx/schema/chrono_traits.hpp"

#include <chrono>

#include <gtest/gtest.h>

using namespace relx::schema;

class TimezoneParsingTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}

  // Helper function to test timezone parsing
  void TestTimezoneConversion(const std::string& input, std::time_t expected_utc_time_t,
                              const std::string& description = "") {
    SCOPED_TRACE("Input: " + input + " - " + description);

    auto parsed = column_traits<std::chrono::system_clock::time_point>::from_sql_string(input);
    auto actual_time_t = std::chrono::system_clock::to_time_t(parsed);

    EXPECT_EQ(actual_time_t, expected_utc_time_t)
        << "Expected UTC time_t: " << expected_utc_time_t << ", Got: " << actual_time_t
        << " (difference: " << (actual_time_t - expected_utc_time_t) << " seconds)";
  }

  void TestTimezoneConversionThrows(const std::string& input, const std::string& description = "") {
    SCOPED_TRACE("Input: " + input + " - " + description);

    EXPECT_THROW(
        { column_traits<std::chrono::system_clock::time_point>::from_sql_string(input); },
        std::invalid_argument);
  }
};

TEST_F(TimezoneParsingTest, BasicUTCFormats) {
  // All should result in the same UTC time: 2023-12-25 10:30:45 UTC
  std::time_t expected_utc = 1703500245;

  TestTimezoneConversion("2023-12-25T10:30:45Z", expected_utc, "ISO with Z");
  TestTimezoneConversion("'2023-12-25T10:30:45Z'", expected_utc, "Quoted ISO with Z");
  TestTimezoneConversion("2023-12-25T10:30:45+00:00", expected_utc, "ISO with +00:00");
  TestTimezoneConversion("2023-12-25T10:30:45-00:00", expected_utc, "ISO with -00:00");
  TestTimezoneConversion("2023-12-25T10:30:45", expected_utc, "ISO without timezone (assume UTC)");
  TestTimezoneConversion("2023-12-25 10:30:45", expected_utc, "Space format without timezone");
}

TEST_F(TimezoneParsingTest, FractionalSecondsWithTimezones) {
  // Test fractional seconds preservation with timezone conversion
  std::time_t base_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  TestTimezoneConversion("2023-12-25T10:30:45.123Z", base_utc, "Fractional seconds with Z");
  TestTimezoneConversion("2023-12-25T15:30:45.123456+05:00", base_utc,
                         "Microseconds with +5 offset");
  TestTimezoneConversion("2023-12-25T05:30:45.999999-05:00", base_utc,
                         "Microseconds with -5 offset");
}

TEST_F(TimezoneParsingTest, PositiveTimezoneOffsets) {
  std::time_t expected_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  // Different ways to represent +5 hours
  TestTimezoneConversion("2023-12-25T15:30:45+05:00", expected_utc, "+5 hours with colon");
  TestTimezoneConversion("2023-12-25T15:30:45+0500", expected_utc, "+5 hours without colon");
  TestTimezoneConversion("2023-12-25T15:30:45+05", expected_utc, "+5 hours only");

  // Various positive offsets
  TestTimezoneConversion("2023-12-25T11:30:45+01:00", expected_utc, "+1 hour");
  TestTimezoneConversion("2023-12-25T13:30:45+03:00", expected_utc, "+3 hours");
  TestTimezoneConversion("2023-12-25T19:30:45+09:00", expected_utc, "+9 hours (Japan)");
  TestTimezoneConversion("2023-12-25T23:30:45+13:00", expected_utc, "+13 hours (extreme)");

  // Half-hour offsets
  TestTimezoneConversion("2023-12-25T16:00:45+05:30", expected_utc, "+5:30 hours (India)");
  TestTimezoneConversion("2023-12-25T15:00:45+04:30", expected_utc, "+4:30 hours");

  // Quarter-hour offsets
  TestTimezoneConversion("2023-12-25T15:15:45+04:45", expected_utc, "+4:45 hours");
  TestTimezoneConversion("2023-12-25T16:15:45+05:45", expected_utc, "+5:45 hours (Nepal)");
}

TEST_F(TimezoneParsingTest, NegativeTimezoneOffsets) {
  std::time_t expected_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  // Different ways to represent -5 hours
  TestTimezoneConversion("2023-12-25T05:30:45-05:00", expected_utc, "-5 hours with colon");
  TestTimezoneConversion("2023-12-25T05:30:45-0500", expected_utc, "-5 hours without colon");
  TestTimezoneConversion("2023-12-25T05:30:45-05", expected_utc, "-5 hours only");

  // Various negative offsets
  TestTimezoneConversion("2023-12-25T09:30:45-01:00", expected_utc, "-1 hour");
  TestTimezoneConversion("2023-12-25T07:30:45-03:00", expected_utc, "-3 hours");
  TestTimezoneConversion("2023-12-25T02:30:45-08:00", expected_utc, "-8 hours (PST)");
  TestTimezoneConversion("2023-12-24T21:30:45-13:00", expected_utc,
                         "-13 hours (extreme, previous day)");

  // Half-hour negative offsets
  TestTimezoneConversion("2023-12-25T05:00:45-05:30", expected_utc, "-5:30 hours");
  TestTimezoneConversion("2023-12-25T06:00:45-04:30", expected_utc, "-4:30 hours");
}

TEST_F(TimezoneParsingTest, EdgeCaseTimezones) {
  std::time_t expected_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  // Maximum positive offset (+14:00)
  TestTimezoneConversion("2023-12-26T00:30:45+14:00", expected_utc, "+14 hours (Line Islands)");

  // Maximum negative offset (-12:00)
  TestTimezoneConversion("2023-12-24T22:30:45-12:00", expected_utc, "-12 hours (Baker Island)");

  // Zero offset variations
  TestTimezoneConversion("2023-12-25T10:30:45+00", expected_utc, "+00 hours");
  TestTimezoneConversion("2023-12-25T10:30:45-00", expected_utc, "-00 hours");
  TestTimezoneConversion("2023-12-25T10:30:45+0000", expected_utc, "+0000");
  TestTimezoneConversion("2023-12-25T10:30:45-0000", expected_utc, "-0000");
}

TEST_F(TimezoneParsingTest, WeirdAndUnusualFormats) {
  std::time_t expected_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  // Quoted strings with various formats
  TestTimezoneConversion("'2023-12-25T15:30:45+05:00'", expected_utc, "Quoted with timezone");
  TestTimezoneConversion("'2023-12-25 15:30:45+05:00'", expected_utc,
                         "Quoted space format with timezone");

  // Single digit hour offsets
  TestTimezoneConversion("2023-12-25T19:30:45+9:00", expected_utc, "Single digit hour +9");
  TestTimezoneConversion("2023-12-25T01:30:45-9:00", expected_utc, "Single digit hour -9");

  // Leading zeros
  TestTimezoneConversion("2023-12-25T15:30:45+05:00", expected_utc, "Standard +05:00");

  // With minutes +05:30: local 16:00:45 - 05:30 = 10:30:45 UTC ✓ (this one is correct)
  TestTimezoneConversion("2023-12-25T16:00:45+05:30", expected_utc, "With minutes +05:30");

  // Space before timezone (PostgreSQL sometimes does this)
  // Note: Our parser looks for last +/-, so this should work
  TestTimezoneConversion("2023-12-25 15:30:45+05", expected_utc, "Space format with timezone");
}

TEST_F(TimezoneParsingTest, CrossDayBoundaries) {
  // Test cases where timezone conversion crosses day boundaries

  // Early morning with negative offset (goes to previous day)
  std::time_t jan1_0030_utc = 1672533000;  // 2023-01-01 00:30:00 UTC
  TestTimezoneConversion("2022-12-31T19:30:00-05:00", jan1_0030_utc, "Previous day with -5");

  // Late night with positive offset (goes to next day)
  std::time_t dec31_2330_utc = 1704065400;  // 2023-12-31 23:30:00 UTC
  TestTimezoneConversion("2024-01-01T09:30:00+10:00", dec31_2330_utc, "Next day with +10");

  // Around midnight UTC
  std::time_t midnight_utc = 1672531200;  // 2023-01-01 00:00:00 UTC
  TestTimezoneConversion("2023-01-01T05:00:00+05:00", midnight_utc, "Local 5am = UTC midnight");
  TestTimezoneConversion("2022-12-31T19:00:00-05:00", midnight_utc,
                         "Local 7pm prev day = UTC midnight");
}

TEST_F(TimezoneParsingTest, LeapYearAndSpecialDates) {
  // Leap year Feb 29
  std::time_t leap_day = 1677628800;  // 2023-03-01 00:00:00 UTC (note: 2023 is not leap year)
  // Let's use 2024 which is a leap year: Feb 29, 2024 12:00:00 UTC = 1709208000
  std::time_t feb29_2024 = 1709208000;  // 2024-02-29 12:00:00 UTC
  TestTimezoneConversion("2024-02-29T17:00:00+05:00", feb29_2024, "Leap year date with timezone");

  // End of year
  std::time_t nye_2023 = 1704064740;  // 2023-12-31 23:19:00 UTC
  TestTimezoneConversion("2024-01-01T04:19:00+05:00", nye_2023, "New Year's with timezone");
}

TEST_F(TimezoneParsingTest, ExtremeFractionalSeconds) {
  std::time_t base_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  // Very precise fractional seconds
  TestTimezoneConversion("2023-12-25T15:30:45.000001+05:00", base_utc, "1 microsecond");
  TestTimezoneConversion("2023-12-25T15:30:45.999999+05:00", base_utc, "999999 microseconds");
  TestTimezoneConversion("2023-12-25T15:30:45.123456789+05:00", base_utc,
                         "Nanoseconds (truncated)");

  // Fractional seconds shorter than 6 digits (should be padded)
  TestTimezoneConversion("2023-12-25T15:30:45.1+05:00", base_utc, "Single fractional digit");
  TestTimezoneConversion("2023-12-25T15:30:45.12+05:00", base_utc, "Two fractional digits");
  TestTimezoneConversion("2023-12-25T15:30:45.123+05:00", base_utc, "Three fractional digits");
}

TEST_F(TimezoneParsingTest, InvalidTimezoneFormats) {
  // These should throw exceptions

  TestTimezoneConversionThrows("2023-12-25T10:30:45+", "Plus sign without offset");
  TestTimezoneConversionThrows("2023-12-25T10:30:45-", "Minus sign without offset");
  TestTimezoneConversionThrows("2023-12-25T10:30:45+25:00", "Invalid hour offset");
  TestTimezoneConversionThrows("2023-12-25T10:30:45+05:60", "Invalid minute offset");
  TestTimezoneConversionThrows("2023-12-25T10:30:45+ABC", "Non-numeric offset");
  TestTimezoneConversionThrows("2023-12-25T10:30:45+5:5:5", "Too many colons");
  TestTimezoneConversionThrows("2023-12-25T10:30:45+123", "Three digit offset");
  TestTimezoneConversionThrows("2023-12-25T10:30:45+12345", "Five digit offset");
}

TEST_F(TimezoneParsingTest, PostgreSQLRealWorldFormats) {
  // Real formats that PostgreSQL might return
  std::time_t expected_utc = 1703500245;  // 2023-12-25 10:30:45 UTC

  TestTimezoneConversion("2023-12-25 15:30:45+05", expected_utc, "PostgreSQL format 1");
  TestTimezoneConversion("2023-12-25 15:30:45.123+05", expected_utc, "PostgreSQL with fractional");
  TestTimezoneConversion("2023-12-25 05:30:45-05", expected_utc, "PostgreSQL negative offset");

  // With timezone names (we strip everything after +/-)
  TestTimezoneConversion("2023-12-25T15:30:45+05:00", expected_utc, "Standard ISO with timezone");
}

TEST_F(TimezoneParsingTest, MidnightAndExtremeTimes) {
  // Test around midnight boundaries
  std::time_t midnight_utc = 1703462400;     // 2023-12-25 00:00:00 UTC
  std::time_t almost_midnight = 1703548799;  // 2023-12-25 23:59:59 UTC

  TestTimezoneConversion("2023-12-25T05:00:00+05:00", midnight_utc, "5am local = midnight UTC");
  TestTimezoneConversion("2023-12-24T19:00:00-05:00", midnight_utc, "7pm prev day = midnight UTC");

  TestTimezoneConversion("2023-12-26T04:59:59+05:00", almost_midnight,
                         "Almost midnight with timezone");
}

TEST_F(TimezoneParsingTest, RoundTripConversion) {
  // Test that our to_sql_string and from_sql_string are compatible

  auto original = std::chrono::system_clock::from_time_t(1703500245);  // 2023-12-25 10:30:45 UTC

  // Convert to string and back
  auto sql_string = column_traits<std::chrono::system_clock::time_point>::to_sql_string(original);
  auto parsed_back = column_traits<std::chrono::system_clock::time_point>::from_sql_string(
      sql_string);

  // Should be the same time (within second precision)
  auto original_time_t = std::chrono::system_clock::to_time_t(original);
  auto parsed_time_t = std::chrono::system_clock::to_time_t(parsed_back);

  EXPECT_EQ(original_time_t, parsed_time_t) << "Round-trip conversion failed";
}