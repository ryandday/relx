#pragma once

#include "core.hpp"

#include <charconv>
#include <chrono>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace relx::schema {

namespace detail {

/// @brief Strict integer parse of a fixed slice; throws on any non-digit content
inline int parse_time_field(std::string_view text, const char* what) {
  int out = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  auto [ptr, ec] = std::from_chars(begin, end, out);
  if (ec != std::errc{} || ptr != end || text.empty()) {
    throw std::invalid_argument("Invalid " + std::string(what) + ": '" + std::string(text) + "'");
  }
  return out;
}

/// @brief Parse "YYYY-MM-DD" (with strict separators) into a validated ymd
inline std::chrono::year_month_day parse_iso_date(std::string_view text,
                                                  const std::string& original) {
  if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
    throw std::invalid_argument("Failed to parse date: " + original);
  }
  const int year = parse_time_field(text.substr(0, 4), "year");
  const int month = parse_time_field(text.substr(5, 2), "month");
  const int day = parse_time_field(text.substr(8, 2), "day");
  const std::chrono::year_month_day ymd{std::chrono::year{year},
                                        std::chrono::month{static_cast<unsigned>(month)},
                                        std::chrono::day{static_cast<unsigned>(day)}};
  if (!ymd.ok()) {
    throw std::invalid_argument("Invalid calendar date: " + original);
  }
  return ymd;
}

}  // namespace detail

/// @brief Column traits for std::chrono::system_clock::time_point.
/// Parsing and formatting are locale-independent and thread-safe (no
/// gmtime/get_time/mktime); malformed input throws std::invalid_argument.
template <>
struct column_traits<std::chrono::system_clock::time_point> {
  static constexpr auto sql_type_name = "TIMESTAMPTZ";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const std::chrono::system_clock::time_point& value) {
    using namespace std::chrono;
    const auto seconds_part = floor<seconds>(value);
    const auto micros = duration_cast<microseconds>(value - seconds_part).count();

    // ISO 8601 with T separator and Z suffix, quoted as a SQL literal
    std::string out = std::format("'{:%FT%T}", seconds_part);
    if (micros > 0) {
      out += std::format(".{:06}", micros);
    }
    out += "Z'";
    return out;
  }

  static std::chrono::system_clock::time_point from_sql_string(const std::string& value) {
    using namespace std::chrono;

    // Accepted forms: 2023-12-25T10:30:45[.ffffff][Z|+HH[[:]MM]|-HH[[:]MM]]
    // with either 'T' or ' ' separating date and time
    std::string_view sv = value;
    if (sv.size() >= 2 && sv.front() == '\'' && sv.back() == '\'') {
      sv = sv.substr(1, sv.size() - 2);  // SQL-literal round-trip form
    }

    if (sv.size() < 19 || (sv[10] != 'T' && sv[10] != ' ') || sv[13] != ':' || sv[16] != ':') {
      throw std::invalid_argument("Failed to parse timestamp: " + value);
    }

    const year_month_day ymd = detail::parse_iso_date(sv.substr(0, 10), value);
    const int hour = detail::parse_time_field(sv.substr(11, 2), "hour");
    const int minute = detail::parse_time_field(sv.substr(14, 2), "minute");
    const int second = detail::parse_time_field(sv.substr(17, 2), "second");
    if (hour > 23 || minute > 59 || second > 60) {
      throw std::invalid_argument("Invalid time of day: " + value);
    }

    std::size_t pos = 19;

    // Fractional seconds, padded/truncated to microseconds
    microseconds fractional{0};
    if (pos < sv.size() && sv[pos] == '.') {
      std::size_t digits_end = pos + 1;
      while (digits_end < sv.size() && sv[digits_end] >= '0' && sv[digits_end] <= '9') {
        ++digits_end;
      }
      std::string frac(sv.substr(pos + 1, digits_end - pos - 1));
      if (frac.empty()) {
        throw std::invalid_argument("Empty fractional seconds: " + value);
      }
      if (frac.size() > 6) {
        frac.resize(6);
      } else {
        frac.append(6 - frac.size(), '0');
      }
      fractional = microseconds{detail::parse_time_field(frac, "fractional seconds")};
      pos = digits_end;
    }

    // Timezone suffix
    minutes tz_offset{0};
    if (pos < sv.size()) {
      const char tz_char = sv[pos];
      if (tz_char == 'Z' && pos == sv.size() - 1) {
        // UTC
      } else if (tz_char == '+' || tz_char == '-') {
        std::string_view offset = sv.substr(pos + 1);
        int hours = 0;
        int mins = 0;
        if (const std::size_t colon = offset.find(':'); colon != std::string_view::npos) {
          hours = detail::parse_time_field(offset.substr(0, colon), "timezone hour");
          mins = detail::parse_time_field(offset.substr(colon + 1), "timezone minute");
        } else if (offset.size() == 4) {
          hours = detail::parse_time_field(offset.substr(0, 2), "timezone hour");
          mins = detail::parse_time_field(offset.substr(2, 2), "timezone minute");
        } else if (offset.size() == 1 || offset.size() == 2) {
          hours = detail::parse_time_field(offset, "timezone hour");
        } else {
          throw std::invalid_argument("Invalid timezone format: " + std::string(sv.substr(pos)));
        }
        if (hours > 14 || mins > 59) {
          throw std::invalid_argument("Timezone offset out of range: " +
                                      std::string(sv.substr(pos)));
        }
        tz_offset = minutes{hours * 60 + mins};
        if (tz_char == '-') {
          tz_offset = -tz_offset;
        }
      } else {
        throw std::invalid_argument("Trailing garbage in timestamp: " + value);
      }
    }

    // UTC_time = wall_time - offset
    const sys_days date{ymd};
    return date + hours{hour} + minutes{minute} + seconds{second} + fractional - tz_offset;
  }
};

/// @brief Column traits for std::chrono::year_month_day. Malformed or special
/// ("infinity") input throws std::invalid_argument instead of yielding garbage.
template <>
struct column_traits<std::chrono::year_month_day> {
  static constexpr auto sql_type_name = "DATE";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const std::chrono::year_month_day& value) {
    return std::format("'{}-{:02}-{:02}'", static_cast<int>(value.year()),
                       static_cast<unsigned>(value.month()), static_cast<unsigned>(value.day()));
  }

  static std::chrono::year_month_day from_sql_string(const std::string& value) {
    std::string_view sv = value;
    if (sv.size() >= 2 && sv.front() == '\'' && sv.back() == '\'') {
      sv = sv.substr(1, sv.size() - 2);  // SQL-literal round-trip form
    }
    return detail::parse_iso_date(sv, value);
  }
};

}  // namespace relx::schema
