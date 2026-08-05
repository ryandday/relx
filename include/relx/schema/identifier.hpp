#pragma once

#include <string>
#include <string_view>

namespace relx::schema {

namespace detail {

/// PostgreSQL reserved keywords (the "reserved" category of the keyword table):
/// identifiers that cannot appear as a table or column name without quoting.
inline constexpr std::string_view reserved_keywords[] = {
    "all",
    "analyse",
    "analyze",
    "and",
    "any",
    "array",
    "as",
    "asc",
    "asymmetric",
    "authorization",
    "binary",
    "both",
    "case",
    "cast",
    "check",
    "collate",
    "collation",
    "column",
    "concurrently",
    "constraint",
    "create",
    "cross",
    "current_catalog",
    "current_date",
    "current_role",
    "current_schema",
    "current_time",
    "current_timestamp",
    "current_user",
    "default",
    "deferrable",
    "desc",
    "distinct",
    "do",
    "else",
    "end",
    "except",
    "false",
    "fetch",
    "for",
    "foreign",
    "freeze",
    "from",
    "full",
    "grant",
    "group",
    "having",
    "ilike",
    "in",
    "initially",
    "inner",
    "intersect",
    "into",
    "is",
    "isnull",
    "join",
    "lateral",
    "leading",
    "left",
    "like",
    "limit",
    "localtime",
    "localtimestamp",
    "natural",
    "not",
    "notnull",
    "null",
    "offset",
    "on",
    "only",
    "or",
    "order",
    "outer",
    "overlaps",
    "placing",
    "primary",
    "references",
    "returning",
    "right",
    "select",
    "session_user",
    "similar",
    "some",
    "symmetric",
    "table",
    "tablesample",
    "then",
    "to",
    "trailing",
    "true",
    "union",
    "unique",
    "user",
    "using",
    "variadic",
    "verbose",
    "when",
    "where",
    "window",
    "with",
};

constexpr bool is_reserved_keyword(std::string_view identifier) {
  for (const std::string_view keyword : reserved_keywords) {
    if (identifier == keyword) {
      return true;
    }
  }
  return false;
}

}  // namespace detail

/// @brief Whether an identifier can be emitted bare: unquoted PostgreSQL identifiers
/// fold to lowercase, so anything outside [a-z_][a-z0-9_]* (or a reserved keyword)
/// must be quoted to keep its identity.
constexpr bool identifier_needs_quoting(std::string_view identifier) {
  if (identifier.empty()) {
    return true;
  }
  const char first = identifier.front();
  if (!((first >= 'a' && first <= 'z') || first == '_')) {
    return true;
  }
  for (const char c : identifier) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
      return true;
    }
  }
  return detail::is_reserved_keyword(identifier);
}

/// @brief Quote an identifier for SQL when it needs it (reserved word, mixed case,
/// special characters); safe lowercase identifiers pass through bare, so generated SQL
/// stays readable. Embedded double quotes are doubled.
constexpr std::string quote_identifier(std::string_view identifier) {
  if (!identifier_needs_quoting(identifier)) {
    return std::string(identifier);
  }
  std::string out;
  out.reserve(identifier.size() + 2);
  out += '"';
  for (const char c : identifier) {
    if (c == '"') {
      out += '"';
    }
    out += c;
  }
  out += '"';
  return out;
}

}  // namespace relx::schema
