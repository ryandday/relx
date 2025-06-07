#pragma once

#include "core.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>

namespace relx::schema {

/// @brief Column traits for std::chrono::system_clock::time_point
template <>
struct column_traits<std::chrono::system_clock::time_point> {
  static constexpr auto sql_type_name = "TIMESTAMPTZ";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const std::chrono::system_clock::time_point& value) {
    // Extract the time_t part and microseconds part
    auto time_t_val = std::chrono::system_clock::to_time_t(value);
    auto time_point_from_time_t = std::chrono::system_clock::from_time_t(time_t_val);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        value - time_point_from_time_t).count();

    // Format as ISO 8601 timestamp with timezone (T separator and Z suffix)
    std::stringstream ss;
    auto tm = *std::gmtime(&time_t_val);
    ss << "'" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    // Add fractional seconds if present
    if (microseconds > 0) {
      ss << "." << std::setfill('0') << std::setw(6) << microseconds;
    }
    
    ss << "Z'";
    return ss.str();
  }

  static std::chrono::system_clock::time_point from_sql_string(const std::string& value) {
    // Handle PostgreSQL TIMESTAMPTZ formats more robustly
    // Common formats: 
    // - 2023-12-25T10:30:45Z
    // - 2023-12-25T10:30:45.123Z  
    // - 2023-12-25T10:30:45+00:00
    // - 2023-12-25 10:30:45+00
    
    std::string clean_value = value;
    std::chrono::microseconds fractional_seconds{0};
    
    // Remove quotes if present
    if (!clean_value.empty() && clean_value.front() == '\'' && clean_value.back() == '\'') {
      clean_value = clean_value.substr(1, clean_value.length() - 2);
    }
    
    // Extract and remove fractional seconds if present
    auto dot_pos = clean_value.find('.');
    if (dot_pos != std::string::npos) {
      auto end_pos = clean_value.find_first_of("Z+-", dot_pos);
      if (end_pos == std::string::npos) {
        end_pos = clean_value.length();
      }
      
      std::string frac_str = clean_value.substr(dot_pos + 1, end_pos - dot_pos - 1);
      // Pad or truncate to 6 digits (microseconds)
      if (frac_str.length() > 6) {
        frac_str = frac_str.substr(0, 6);
      } else {
        frac_str.append(6 - frac_str.length(), '0');
      }
      
      fractional_seconds = std::chrono::microseconds{std::stoi(frac_str)};
      clean_value.erase(dot_pos, end_pos - dot_pos);
      if (end_pos < value.length()) {
        clean_value += value.substr(end_pos);
      }
    }
    
    // Parse timezone information and calculate UTC offset
    std::chrono::minutes timezone_offset{0};  // Offset to subtract from local time to get UTC
    
    // Look for timezone info at the end of the string
    auto tz_pos = clean_value.find_last_of("Z");
    if (tz_pos != std::string::npos && tz_pos == clean_value.length() - 1) {
      // Found Z at the end - this means UTC, no offset needed
      clean_value = clean_value.substr(0, tz_pos);
    } else {
      // Look for +/- timezone offset (e.g. +05:00, -08:00, +0530)
      tz_pos = clean_value.find_last_of("+-");
      if (tz_pos != std::string::npos && tz_pos > 10) { // Make sure it's not part of the date
        std::string tz_str = clean_value.substr(tz_pos);
        clean_value = clean_value.substr(0, tz_pos);
        
        // Parse timezone offset: +05:00, -08:00, +0530, etc.
        bool is_positive = (tz_str[0] == '+');
        std::string offset_str = tz_str.substr(1); // Remove +/- sign
        
        if (offset_str.empty()) {
          throw std::invalid_argument("Empty timezone offset: " + tz_str);
        }
        
        int hours = 0, minutes = 0;
        
        try {
          if (offset_str.find(':') != std::string::npos) {
            // Format: HH:MM or H:MM
            auto colon_pos = offset_str.find(':');
            if (offset_str.find(':', colon_pos + 1) != std::string::npos) {
              throw std::invalid_argument("Too many colons in timezone: " + tz_str);
            }
            hours = std::stoi(offset_str.substr(0, colon_pos));
            auto minute_str = offset_str.substr(colon_pos + 1);
            if (minute_str.empty()) {
              throw std::invalid_argument("Missing minutes after colon: " + tz_str);
            }
            minutes = std::stoi(minute_str);
          } else if (offset_str.length() == 4) {
            // Format: HHMM
            hours = std::stoi(offset_str.substr(0, 2));
            minutes = std::stoi(offset_str.substr(2, 2));
          } else if (offset_str.length() == 2) {
            // Format: HH
            hours = std::stoi(offset_str);
          } else if (offset_str.length() == 1) {
            // Format: H (single digit hour)
            hours = std::stoi(offset_str);
          } else {
            throw std::invalid_argument("Invalid timezone format: " + tz_str);
          }
        } catch (const std::invalid_argument& e) {
          throw std::invalid_argument("Invalid timezone format: " + tz_str);
        } catch (const std::out_of_range& e) {
          throw std::invalid_argument("Timezone values out of range: " + tz_str);
        }
        
        // Validate timezone values
        if (hours > 14 || hours < 0) {
          throw std::invalid_argument("Invalid timezone hour offset (must be 0-14): " + tz_str);
        }
        if (minutes >= 60 || minutes < 0) {
          throw std::invalid_argument("Invalid timezone minute offset (must be 0-59): " + tz_str);
        }
        
        // Calculate total offset in minutes
        int total_minutes = hours * 60 + minutes;
        if (!is_positive) {
          total_minutes = -total_minutes;
        }
        
        // To convert to UTC: UTC_time = local_time - offset
        // So if we have +05:00, we subtract 5 hours to get UTC
        timezone_offset = std::chrono::minutes{total_minutes};
      }
      // If no timezone info found, assume UTC (timezone_offset remains 0)
    }
    
    // Parse the base timestamp
    std::tm tm = {};
    std::istringstream ss(clean_value);
    
    bool parsed = false;
    
    // Try ISO format first: YYYY-MM-DDTHH:MM:SS
    if (clean_value.find('T') != std::string::npos) {
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
      parsed = !ss.fail();
    }
    
    // Try space-separated format: YYYY-MM-DD HH:MM:SS
    if (!parsed) {
      ss.clear();
      ss.str(clean_value);
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      parsed = !ss.fail();
    }
    
    if (!parsed) {
      throw std::invalid_argument("Failed to parse timestamp: " + value);
    }
    
    // Convert to time_point (treating as UTC since PostgreSQL TIMESTAMPTZ is timezone-aware)
    // Set remaining fields properly
    tm.tm_isdst = 0;  // Not DST since we're working with UTC
    tm.tm_wday = 0;   // Day of week (not used for conversion)
    tm.tm_yday = 0;   // Day of year (not used for conversion)
    
    // Convert using mktime but adjust for UTC
    // Since mktime assumes local time, we need to convert to UTC
    std::time_t time_t_val;
    
#ifdef _WIN32
    // Windows has _mkgmtime for UTC conversion
    time_t_val = _mkgmtime(&tm);
#else
    // Unix-like systems: use timegm if available, otherwise calculate offset
    #if defined(__GLIBC__) || defined(__APPLE__) || defined(__FreeBSD__)
        time_t_val = timegm(&tm);
    #else
        // Fallback: use mktime and adjust for timezone
        time_t_val = mktime(&tm);
        if (time_t_val != -1) {
            // Get timezone offset to adjust to UTC
            auto gmt_tm = *gmtime(&time_t_val);
            auto local_tm = *localtime(&time_t_val);
            
            // Calculate the offset between local time and GMT
            auto gmt_time = mktime(&gmt_tm);
            auto local_time = mktime(&local_tm);
            auto offset = local_time - gmt_time;
            time_t_val -= offset;
        }
    #endif
#endif
    
    if (time_t_val == -1) {
      throw std::invalid_argument("Invalid timestamp value: " + value);
    }
    
    auto time_point = std::chrono::system_clock::from_time_t(time_t_val);
    
    // Apply timezone offset to convert to UTC
    // timezone_offset is the offset to subtract from local time to get UTC
    auto utc_time_point = time_point - timezone_offset;
    
    return utc_time_point + fractional_seconds;
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