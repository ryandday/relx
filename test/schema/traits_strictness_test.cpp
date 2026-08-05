#include <chrono>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Regression tests for the 2026-08-05 hardening pass: strict numeric parsing,
// locale-free chrono handling, and conninfo escaping.

namespace {

using relx::schema::column_traits;

TEST(NumericTraitsStrictness, PartialParsesRejected) {
  EXPECT_EQ(column_traits<int>::from_sql_string("12"), 12);
  EXPECT_THROW(column_traits<int>::from_sql_string("12abc"), std::invalid_argument);
  EXPECT_THROW(column_traits<int>::from_sql_string(""), std::invalid_argument);
  EXPECT_THROW(column_traits<long>::from_sql_string("1 "), std::invalid_argument);
  EXPECT_THROW(column_traits<double>::from_sql_string("3.5x"), std::invalid_argument);
}

TEST(NumericTraitsStrictness, DoubleFormatsShortestRoundTrip) {
  // std::to_string would emit "0.100000" and truncate precision at 6 digits
  EXPECT_EQ(column_traits<double>::to_sql_string(0.1), "0.1");
  const double precise = 1299.9899999999998;
  EXPECT_EQ(column_traits<double>::from_sql_string(column_traits<double>::to_sql_string(precise)),
            precise);
}

TEST(ChronoTraits, TimestampParsesAcceptedForms) {
  using std::chrono::sys_days;
  using namespace std::chrono;

  const auto expected = sys_days{year{2023} / 12 / 25} + hours{10} + minutes{30} + seconds{45};

  EXPECT_EQ(column_traits<system_clock::time_point>::from_sql_string("2023-12-25T10:30:45Z"),
            expected);
  EXPECT_EQ(column_traits<system_clock::time_point>::from_sql_string("2023-12-25 10:30:45"),
            expected);
  // +05:00 wall time is 05:30 earlier in UTC... i.e. UTC = wall - offset
  EXPECT_EQ(column_traits<system_clock::time_point>::from_sql_string("2023-12-25 15:30:45+05"),
            expected);
  EXPECT_EQ(column_traits<system_clock::time_point>::from_sql_string("2023-12-25T05:30:45-05:00"),
            expected);
  EXPECT_EQ(column_traits<system_clock::time_point>::from_sql_string("2023-12-25T10:30:45.250000Z"),
            expected + milliseconds{250});
}

TEST(ChronoTraits, TimestampRoundTripsThroughSqlLiteral) {
  using namespace std::chrono;
  const auto value = sys_days{year{2024} / 2 / 29} + hours{23} + minutes{59} + seconds{59} +
                     microseconds{123456};
  const std::string literal = column_traits<system_clock::time_point>::to_sql_string(value);
  EXPECT_EQ(column_traits<system_clock::time_point>::from_sql_string(literal), value);
}

TEST(ChronoTraits, MalformedTimestampThrows) {
  using tp_traits = column_traits<std::chrono::system_clock::time_point>;
  EXPECT_THROW(tp_traits::from_sql_string("not a date"), std::invalid_argument);
  EXPECT_THROW(tp_traits::from_sql_string("2023-13-45T99:99:99Z"), std::invalid_argument);
  EXPECT_THROW(tp_traits::from_sql_string("2023-12-25T10:30:45garbage"), std::invalid_argument);
  EXPECT_THROW(tp_traits::from_sql_string("infinity"), std::invalid_argument);
}

TEST(ChronoTraits, DateValidatesCalendar) {
  using std::chrono::year_month_day;
  using ymd_traits = column_traits<year_month_day>;
  using namespace std::chrono;

  EXPECT_EQ(ymd_traits::from_sql_string("2024-02-29"), year{2024} / 2 / 29);
  EXPECT_THROW(ymd_traits::from_sql_string("2023-02-29"),
               std::invalid_argument);  // not a leap year
  EXPECT_THROW(ymd_traits::from_sql_string("infinity"), std::invalid_argument);
  EXPECT_THROW(ymd_traits::from_sql_string("2023-1-1"), std::invalid_argument);  // shape mismatch
  EXPECT_THROW(ymd_traits::from_sql_string("garbage"), std::invalid_argument);
}

TEST(ChronoTraits, YmdValueBindsUnquotedText) {
  using namespace std::chrono;
  auto params = relx::query::val(year{2024} / 3 / 15).bind_params();
  ASSERT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "2024-03-15");
}

TEST(ConnectionParams, ConninfoValuesAreQuotedAndEscaped) {
  relx::connection::PostgreSQLConnectionParams params;
  params.host = "localhost";
  params.dbname = "mydb";
  params.user = "svc account";      // space
  params.password = "p'ass\\word";  // quote and backslash

  const std::string conninfo = params.to_connection_string();
  EXPECT_NE(conninfo.find("user='svc account'"), std::string::npos) << conninfo;
  EXPECT_NE(conninfo.find("password='p\\'ass\\\\word'"), std::string::npos) << conninfo;
}

}  // namespace
