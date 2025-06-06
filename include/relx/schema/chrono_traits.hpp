#pragma once

#include "core.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace relx::schema {

/// @brief Column traits for std::chrono::system_clock::time_point
template <>
struct column_traits<std::chrono::system_clock::time_point> {
  static constexpr auto sql_type_name = "TIMESTAMPTZ";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const std::chrono::system_clock::time_point& value) {
    // Convert to time_t for formatting
    auto time_t_val = std::chrono::system_clock::to_time_t(value);

    // Format as ISO 8601 timestamp with timezone (T separator and Z suffix)
    std::stringstream ss;
    auto tm = *std::gmtime(&time_t_val);
    ss << std::put_time(&tm, "'%Y-%m-%dT%H:%M:%SZ'");
    return ss.str();
  }

  static std::chrono::system_clock::time_point from_sql_string(const std::string& value) {
    // For PostgreSQL, we'll receive an ISO format string
    // This is a simplified parser - in production you'd want a more robust one
    std::tm tm = {};
    std::istringstream ss(value);

    // Try to parse common PostgreSQL timestamp formats
    if (value.find('T') != std::string::npos) {
      // ISO format: YYYY-MM-DDTHH:MM:SS
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    } else {
      // Space format: YYYY-MM-DD HH:MM:SS
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    // Convert to time_point
    auto time_t_val = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t_val);
  }
};

/// @brief Column traits for std::chrono::year_month_day
template <>
struct column_traits<std::chrono::year_month_day> {
  static constexpr auto sql_type_name = "DATE";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const std::chrono::year_month_day& value) {
    std::stringstream ss;
    ss << "'" << static_cast<int>(value.year()) << "-" << std::setfill('0') << std::setw(2)
       << static_cast<unsigned>(value.month()) << "-" << std::setfill('0') << std::setw(2)
       << static_cast<unsigned>(value.day()) << "'";
    return ss.str();
  }

  static std::chrono::year_month_day from_sql_string(const std::string& value) {
    // Parse YYYY-MM-DD format
    int year, month, day;
    char dash1, dash2;

    std::istringstream ss(value);
    ss >> year >> dash1 >> month >> dash2 >> day;

    return std::chrono::year_month_day{std::chrono::year{year},
                                       std::chrono::month{static_cast<unsigned>(month)},
                                       std::chrono::day{static_cast<unsigned>(day)}};
  }
};

}  // namespace relx::schema