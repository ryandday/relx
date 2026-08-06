#pragma once

#include "schema/fixed_string.hpp"

#include <cstddef>
#include <string>
#include <string_view>

/// @brief Compile-time validation for raw SQL literals with $n placeholders:
///
/// ```cpp
/// conn.execute_raw(std::string(relx::checked_sql<"SELECT * FROM users WHERE id = $1", 1>()),
///                  {id});
/// ```
///
/// The placeholder set must be dense ($1..$N with no gaps - $1 and $3 without $2 is a
/// typo the server would report only at runtime), and when a count is supplied it must
/// match. Placeholders inside single-quoted strings, double-quoted identifiers, line
/// comments and block comments are ignored, matching PostgreSQL's lexing.
namespace relx {

namespace detail {

struct placeholder_scan {
  std::size_t max_index = 0;
  unsigned long long seen_mask = 0;  ///< bit n-1 set when $n was seen (n <= 64)
  bool overflow = false;             ///< a placeholder above $64 appeared
  bool zero = false;                 ///< $0 appeared (invalid)
};

/// @brief Scan $n placeholders outside strings/identifiers/comments
consteval placeholder_scan scan_placeholders(std::string_view sql) {
  placeholder_scan result;
  std::size_t i = 0;
  while (i < sql.size()) {
    const char c = sql[i];
    if (c == '\'') {
      // single-quoted string; '' is an escaped quote
      ++i;
      while (i < sql.size()) {
        if (sql[i] == '\'') {
          if (i + 1 < sql.size() && sql[i + 1] == '\'') {
            i += 2;
            continue;
          }
          ++i;
          break;
        }
        ++i;
      }
    } else if (c == '"') {
      // double-quoted identifier; "" is an escaped quote
      ++i;
      while (i < sql.size()) {
        if (sql[i] == '"') {
          if (i + 1 < sql.size() && sql[i + 1] == '"') {
            i += 2;
            continue;
          }
          ++i;
          break;
        }
        ++i;
      }
    } else if (c == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
      while (i < sql.size() && sql[i] != '\n') {
        ++i;
      }
    } else if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
      i += 2;
      while (i + 1 < sql.size() && !(sql[i] == '*' && sql[i + 1] == '/')) {
        ++i;
      }
      i = (i + 1 < sql.size()) ? i + 2 : sql.size();
    } else if (c == '$' && i + 1 < sql.size() && sql[i + 1] >= '0' && sql[i + 1] <= '9') {
      std::size_t index = 0;
      ++i;
      while (i < sql.size() && sql[i] >= '0' && sql[i] <= '9') {
        index = index * 10 + static_cast<std::size_t>(sql[i] - '0');
        ++i;
      }
      if (index == 0) {
        result.zero = true;
      } else if (index > 64) {
        result.overflow = true;
      } else {
        result.seen_mask |= 1ull << (index - 1);
        if (index > result.max_index) {
          result.max_index = index;
        }
      }
    } else {
      ++i;
    }
  }
  return result;
}

consteval bool placeholders_dense(const placeholder_scan& scan) {
  if (scan.zero || scan.overflow) {
    return false;
  }
  for (std::size_t n = 1; n <= scan.max_index; ++n) {
    if ((scan.seen_mask & (1ull << (n - 1))) == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace detail

/// @brief The number of distinct $n placeholders in a SQL literal, validated dense
template <schema::fixed_string Sql>
consteval std::size_t placeholder_count() {
  constexpr auto scan = detail::scan_placeholders(std::string_view(Sql));
  static_assert(!scan.zero, "raw SQL uses $0: placeholders start at $1");
  static_assert(!scan.overflow, "raw SQL uses a placeholder above $64");
  static_assert(detail::placeholders_dense(scan),
                "raw SQL placeholders have a gap: $1..$N must all appear (a skipped index "
                "is almost always a typo, and the server would reject the parameter count)");
  return scan.max_index;
}

/// @brief The validated SQL text of a raw literal (placeholders checked dense)
template <schema::fixed_string Sql>
consteval std::string_view checked_sql() {
  (void)placeholder_count<Sql>();
  return std::string_view(Sql);
}

/// @brief The validated SQL text, additionally asserting the placeholder count -
/// keeps the literal and the parameter list it is executed with in sync at the
/// call site
template <schema::fixed_string Sql, std::size_t ExpectedParams>
consteval std::string_view checked_sql() {
  static_assert(placeholder_count<Sql>() == ExpectedParams,
                "raw SQL placeholder count does not match the declared parameter count");
  return std::string_view(Sql);
}

}  // namespace relx
