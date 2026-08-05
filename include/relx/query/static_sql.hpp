#pragma once

#include "core.hpp"

#include <meta>
#include <string>
#include <string_view>

/// @brief Compile-time SQL rendering for typed queries.
///
/// Since bind parameters carry values out-of-band (a disengaged optional binds a NULL
/// parameter instead of splicing the literal into the statement), the SQL text of a
/// typed query is value-independent. static_sql evaluates the whole builder chain at
/// compile time and stores the statement in static storage:
///
/// ```cpp
/// constexpr auto users = relx::t<Users>;
/// constexpr std::string_view sql =
///     relx::static_sql(select(users.id).from(users).where(users.id == 42));
/// static_assert(sql == "SELECT users.id FROM users WHERE (users.id = ?)");
/// ```
///
/// Requirements: every table object referenced must be usable in a constant expression
/// (relx::t<T> and constexpr classic table instances qualify), and the query must not
/// contain runtime-only nodes (aliased columns hold a shared_ptr; IN-lists have a
/// value-dependent placeholder count). Such queries simply fail to constant-evaluate -
/// their runtime to_sql() is unaffected.
namespace relx::query {

/// @brief The query's SQL rendered at compile time, backed by static storage
template <SqlExpr Query>
consteval std::string_view static_sql(const Query& query) {
  return std::define_static_string(query.to_sql());
}

/// @brief The query's SQL with PostgreSQL placeholders ($1, $2, ...) instead of ?,
/// rendered at compile time. Mirrors the runtime conversion in
/// connection/sql_utils.cpp: ? inside single-quoted literals or double-quoted
/// identifiers is left alone.
template <SqlExpr Query>
consteval std::string_view static_pg_sql(const Query& query) {
  const std::string sql = query.to_sql();
  std::string out;
  int placeholder = 1;
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  for (std::size_t i = 0; i < sql.size(); ++i) {
    const char current = sql[i];
    if (current == '\'' && !in_double_quotes) {
      if (i + 1 < sql.size() && sql[i + 1] == '\'') {
        out += current;
        out += sql[++i];
        continue;
      }
      in_single_quotes = !in_single_quotes;
    } else if (current == '"' && !in_single_quotes) {
      if (i + 1 < sql.size() && sql[i + 1] == '"') {
        out += current;
        out += sql[++i];
        continue;
      }
      in_double_quotes = !in_double_quotes;
    } else if (current == '?' && !in_single_quotes && !in_double_quotes) {
      out += '$';
      // consteval-friendly int-to-string (std::to_string is not constexpr)
      char digits[8] = {};
      int n = placeholder++;
      int len = 0;
      while (n > 0) {
        digits[len++] = static_cast<char>('0' + n % 10);
        n /= 10;
      }
      while (len > 0) {
        out += digits[--len];
      }
      continue;
    }
    out += current;
  }

  return std::define_static_string(out);
}

}  // namespace relx::query

namespace relx {
using query::static_pg_sql;
using query::static_sql;
}  // namespace relx
