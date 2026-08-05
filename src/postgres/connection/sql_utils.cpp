#include "relx/connection/sql_utils.hpp"

#include "relx/results.hpp"

#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

#include <libpq-fe.h>

namespace relx::connection::sql_utils {

namespace {

/// A dollar-quote tag starting at sql[pos] ("$$" or "$tag$"), or empty if none
std::string_view dollar_quote_tag(std::string_view sql, std::size_t pos) {
  if (pos >= sql.size() || sql[pos] != '$') {
    return {};
  }
  std::size_t end = pos + 1;
  while (end < sql.size() &&
         (sql[end] == '_' || (std::isalnum(static_cast<unsigned char>(sql[end])) != 0))) {
    ++end;
  }
  if (end < sql.size() && sql[end] == '$') {
    return sql.substr(pos, end - pos + 1);  // includes both '$'s
  }
  return {};
}

}  // namespace

std::string convert_placeholders_to_postgresql(const std::string& sql) {
  std::string result;
  result.reserve(sql.size() + 32);  // Reserve some extra space for parameter numbers

  int placeholder_count = 1;

  for (size_t i = 0; i < sql.size(); ++i) {
    const char current = sql[i];
    const char next = (i + 1 < sql.size()) ? sql[i + 1] : '\0';

    // Line comments: -- to end of line
    if (current == '-' && next == '-') {
      const std::size_t eol = sql.find('\n', i);
      const std::size_t end = (eol == std::string::npos) ? sql.size() : eol + 1;
      result.append(sql, i, end - i);
      i = end - 1;
      continue;
    }

    // Block comments: /* ... */, which PostgreSQL nests
    if (current == '/' && next == '*') {
      int depth = 1;
      std::size_t j = i + 2;
      while (j < sql.size() && depth > 0) {
        if (sql[j] == '/' && j + 1 < sql.size() && sql[j + 1] == '*') {
          ++depth;
          j += 2;
        } else if (sql[j] == '*' && j + 1 < sql.size() && sql[j + 1] == '/') {
          --depth;
          j += 2;
        } else {
          ++j;
        }
      }
      result.append(sql, i, j - i);
      i = j - 1;
      continue;
    }

    // E'...' strings: backslash escapes a quote inside
    if ((current == 'E' || current == 'e') && next == '\'' &&
        (i == 0 ||
         (std::isalnum(static_cast<unsigned char>(sql[i - 1])) == 0 && sql[i - 1] != '_'))) {
      std::size_t j = i + 2;
      while (j < sql.size()) {
        if (sql[j] == '\\' && j + 1 < sql.size()) {
          j += 2;
          continue;
        }
        if (sql[j] == '\'') {
          if (j + 1 < sql.size() && sql[j + 1] == '\'') {
            j += 2;  // doubled quote inside the string
            continue;
          }
          ++j;  // closing quote
          break;
        }
        ++j;
      }
      result.append(sql, i, j - i);
      i = j - 1;
      continue;
    }

    // Ordinary '...' strings: '' escapes a quote inside
    if (current == '\'') {
      std::size_t j = i + 1;
      while (j < sql.size()) {
        if (sql[j] == '\'') {
          if (j + 1 < sql.size() && sql[j + 1] == '\'') {
            j += 2;
            continue;
          }
          ++j;
          break;
        }
        ++j;
      }
      result.append(sql, i, j - i);
      i = j - 1;
      continue;
    }

    // "..." quoted identifiers: "" escapes a quote inside
    if (current == '"') {
      std::size_t j = i + 1;
      while (j < sql.size()) {
        if (sql[j] == '"') {
          if (j + 1 < sql.size() && sql[j + 1] == '"') {
            j += 2;
            continue;
          }
          ++j;
          break;
        }
        ++j;
      }
      result.append(sql, i, j - i);
      i = j - 1;
      continue;
    }

    // Dollar-quoted strings: $tag$ ... $tag$ (no escapes inside)
    if (const std::string_view tag = dollar_quote_tag(sql, i); !tag.empty()) {
      const std::size_t body = i + tag.size();
      const std::size_t close = sql.find(std::string(tag), body);
      const std::size_t end = (close == std::string::npos) ? sql.size() : close + tag.size();
      result.append(sql, i, end - i);
      i = end - 1;
      continue;
    }

    // ?? is an escaped literal '?' (e.g. the JSONB ? operator); emit a single '?'
    if (current == '?' && next == '?') {
      result += '?';
      ++i;
      continue;
    }

    // A lone ? is a parameter placeholder
    if (current == '?') {
      result += '$';
      result += std::to_string(placeholder_count++);
      continue;
    }

    result += current;
  }

  return result;
}

std::string isolation_level_to_postgresql_string(int isolation_level) {
  switch (isolation_level) {
  case 0:  // IsolationLevel::ReadUncommitted
    return "READ UNCOMMITTED";
  case 1:  // IsolationLevel::ReadCommitted
    return "READ COMMITTED";
  case 2:  // IsolationLevel::RepeatableRead
    return "REPEATABLE READ";
  case 3:  // IsolationLevel::Serializable
    return "SERIALIZABLE";
  default:
    return "READ COMMITTED";
  }
}

// Helper function to convert PostgreSQL hex BYTEA format to binary.
// Malformed hex throws std::invalid_argument - silently returning the hex text
// as the value would corrupt the data.
static std::string convert_pg_bytea_to_binary(const std::string& hex_value) {
  // Check if this is a PostgreSQL hex-encoded BYTEA value (starts with \x)
  if (hex_value.size() < 2 || hex_value.substr(0, 2) != "\\x") {
    // Not in hex format, return as is
    return hex_value;
  }

  if (hex_value.size() % 2 != 0) {
    throw std::invalid_argument("BYTEA hex value has odd length");
  }

  const auto hex_digit = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    throw std::invalid_argument(std::string("Invalid BYTEA hex digit: '") + c + "'");
  };

  std::string binary_result;
  binary_result.reserve((hex_value.size() - 2) / 2);
  for (size_t i = 2; i + 1 < hex_value.size(); i += 2) {
    binary_result.push_back(
        static_cast<char>((hex_digit(hex_value[i]) << 4) | hex_digit(hex_value[i + 1])));
  }
  return binary_result;
}

result::ResultSet process_postgresql_result(PGresult* pg_result, bool convert_bytea) {
  std::vector<std::string> column_names;
  std::vector<result::Row> rows;

  // Get column names
  const int column_count = PQnfields(pg_result);
  column_names.reserve(column_count);
  for (int i = 0; i < column_count; i++) {
    const char* name = PQfname(pg_result, i);
    column_names.push_back(name ? name : "");
  }

  // Process rows
  const int row_count = PQntuples(pg_result);
  rows.reserve(row_count);

  // Get column types to identify BYTEA columns if conversion is enabled
  std::vector<bool> is_bytea_column(column_count, false);
  if (convert_bytea) {
    for (int i = 0; i < column_count; i++) {
      // PostgreSQL BYTEA type OID is 17
      is_bytea_column[i] = (PQftype(pg_result, i) == 17);
    }
  }

  for (int row_idx = 0; row_idx < row_count; row_idx++) {
    std::vector<result::Cell> cells;
    cells.reserve(column_count);

    for (int col_idx = 0; col_idx < column_count; col_idx++) {
      if (PQgetisnull(pg_result, row_idx, col_idx)) {
        cells.push_back(result::Cell::null());
      } else {
        const char* value = PQgetvalue(pg_result, row_idx, col_idx);
        std::string cell_value = value ? value : "";

        // Automatically convert BYTEA data from hex to binary if enabled
        if (convert_bytea && is_bytea_column[col_idx]) {
          cell_value = convert_pg_bytea_to_binary(cell_value);
        }

        cells.emplace_back(std::move(cell_value));
      }
    }

    rows.push_back(result::Row(std::move(cells), column_names));
  }

  // Create the result set from the data we collected
  return result::ResultSet(std::move(rows), std::move(column_names));
}

namespace {

std::uint64_t read_big_endian(const unsigned char* bytes, int len) {
  std::uint64_t bits = 0;
  for (int i = 0; i < len; ++i) {
    bits = (bits << 8) | bytes[i];
  }
  return bits;
}

// Match PostgreSQL's text output: special values by name, finite values in
// shortest-round-trip form (PostgreSQL's default since v12). Must format at the wire
// precision - promoting a float4 to double first would print the float's exact double
// value ("1299.989990234375") instead of the float's shortest form ("1299.99").
template <typename F>
std::string format_float_like_postgres(F value) {
  if (std::isnan(value)) {
    return "NaN";
  }
  if (std::isinf(value)) {
    return value > 0 ? "Infinity" : "-Infinity";
  }
  return std::format("{}", value);
}

/// Decode one binary cell into the text form the text protocol would have produced,
/// or an error string
std::expected<std::string, std::string> decode_binary_cell(Oid type_oid, const char* data,
                                                           int len) {
  const auto* bytes = reinterpret_cast<const unsigned char*>(data);
  constexpr Oid first_user_defined_oid = 16384;

  switch (type_oid) {
  case 16:  // bool
    if (len != 1) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for bool");
    }
    return std::string(bytes[0] != 0 ? "t" : "f");

  case 21:  // int2
  case 23:  // int4
  case 20:  // int8
    if (len != 2 && len != 4 && len != 8) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for integer");
    }
    {
      const std::uint64_t bits = read_big_endian(bytes, len);
      // Sign-extend from the wire width
      const int shift = 64 - 8 * len;
      const auto value = static_cast<std::int64_t>(bits << shift) >> shift;
      return std::to_string(value);
    }

  case 700:  // float4
    if (len != 4) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for float4");
    }
    return format_float_like_postgres(
        std::bit_cast<float>(static_cast<std::uint32_t>(read_big_endian(bytes, 4))));

  case 701:  // float8
    if (len != 8) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for float8");
    }
    return format_float_like_postgres(std::bit_cast<double>(read_big_endian(bytes, 8)));

  case 25:    // text
  case 1042:  // bpchar
  case 1043:  // varchar
  case 19:    // name
  case 705:   // unknown
  case 17:    // bytea - binary format is already the raw bytes
    return std::string(data, static_cast<std::size_t>(len));

  case 1082: {  // date: days since 2000-01-01
    if (len != 4) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for date");
    }
    const auto days = static_cast<std::int32_t>(read_big_endian(bytes, 4));
    if (days == std::numeric_limits<std::int32_t>::max()) {
      return std::string("infinity");
    }
    if (days == std::numeric_limits<std::int32_t>::min()) {
      return std::string("-infinity");
    }
    const std::chrono::sys_days date{std::chrono::year{2000} / 1 / 1};
    return std::format("{:%F}", date + std::chrono::days{days});
  }

  case 1114:    // timestamp: microseconds since 2000-01-01
  case 1184: {  // timestamptz: microseconds since 2000-01-01 UTC
    if (len != 8) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for timestamp");
    }
    const auto micros = static_cast<std::int64_t>(read_big_endian(bytes, 8));
    if (micros == std::numeric_limits<std::int64_t>::max()) {
      return std::string("infinity");
    }
    if (micros == std::numeric_limits<std::int64_t>::min()) {
      return std::string("-infinity");
    }
    const std::chrono::sys_time<std::chrono::microseconds> ts{
        std::chrono::sys_days{std::chrono::year{2000} / 1 / 1} + std::chrono::microseconds{micros}};
    std::string text = std::format("{:%F %T}", ts);
    if (type_oid == 1184) {
      text += "+00";  // decoded as UTC; the parser understands the offset suffix
    }
    return text;
  }

  case 1700: {  // numeric: base-10000 digit groups (mirrors PostgreSQL's get_str_from_var)
    if (len < 8) {
      return std::unexpected("truncated numeric value");
    }
    const auto read16 = [bytes](int offset) {
      return static_cast<std::uint16_t>(read_big_endian(bytes + offset, 2));
    };
    const int ndigits = read16(0);
    const auto weight = static_cast<std::int16_t>(read16(2));
    const std::uint16_t sign = read16(4);
    const int dscale = read16(6) & 0x3FFF;
    if (len != 8 + ndigits * 2) {
      return std::unexpected("truncated numeric value");
    }
    if (sign == 0xC000) {
      return std::string("NaN");
    }
    if (sign == 0xD000) {
      return std::string("Infinity");
    }
    if (sign == 0xF000) {
      return std::string("-Infinity");
    }

    const auto digit_group = [&](int i) {
      return (i >= 0 && i < ndigits) ? static_cast<int>(read16(8 + 2 * i)) : 0;
    };
    std::string out = (sign == 0x4000) ? "-" : "";
    if (weight < 0) {
      out += "0";
    } else {
      for (int i = 0; i <= weight; ++i) {
        const std::string group = std::to_string(digit_group(i));
        out += (i == 0) ? group : std::string(4 - group.size(), '0') + group;
      }
    }
    if (dscale > 0) {
      std::string fraction;
      for (int i = weight + 1; std::cmp_less(fraction.size(), dscale); ++i) {
        const std::string group = std::to_string(digit_group(i));
        fraction += std::string(4 - group.size(), '0') + group;
      }
      fraction.resize(dscale);
      out += "." + fraction;
    }
    return out;
  }

  case 2950: {  // uuid: 16 raw bytes, formatted 8-4-4-4-12
    if (len != 16) {
      return std::unexpected("unexpected length " + std::to_string(len) + " for uuid");
    }
    std::string out;
    out.reserve(36);
    for (int i = 0; i < 16; ++i) {
      if (i == 4 || i == 6 || i == 8 || i == 10) {
        out += '-';
      }
      out += std::format("{:02x}", static_cast<unsigned char>(bytes[i]));
    }
    return out;
  }

  default:
    if (type_oid >= first_user_defined_oid) {
      // User-defined type: for enum types (the only user-defined types relx creates)
      // the binary representation is the label text
      return std::string(data, static_cast<std::size_t>(len));
    }
    return std::unexpected("type OID " + std::to_string(type_oid) +
                           " cannot be decoded from binary result format");
  }
}

}  // namespace

std::expected<result::ResultSet, std::string> process_postgresql_result_binary(
    PGresult* pg_result) {
  std::vector<std::string> column_names;
  const int column_count = PQnfields(pg_result);
  column_names.reserve(column_count);
  for (int i = 0; i < column_count; i++) {
    const char* name = PQfname(pg_result, i);
    column_names.push_back(name ? name : "");
  }

  const int row_count = PQntuples(pg_result);
  std::vector<result::Row> rows;
  rows.reserve(row_count);

  for (int row_idx = 0; row_idx < row_count; row_idx++) {
    std::vector<result::Cell> cells;
    cells.reserve(column_count);

    for (int col_idx = 0; col_idx < column_count; col_idx++) {
      if (PQgetisnull(pg_result, row_idx, col_idx)) {
        cells.push_back(result::Cell::null());
        continue;
      }
      auto decoded = decode_binary_cell(PQftype(pg_result, col_idx),
                                        PQgetvalue(pg_result, row_idx, col_idx),
                                        PQgetlength(pg_result, row_idx, col_idx));
      if (!decoded) {
        return std::unexpected("column '" + column_names[col_idx] + "': " + decoded.error());
      }
      cells.emplace_back(std::move(*decoded));
    }

    rows.push_back(result::Row(std::move(cells), column_names));
  }

  return result::ResultSet(std::move(rows), std::move(column_names));
}

std::expected<std::string, std::string> decode_binary_cell_for_testing(unsigned int type_oid,
                                                                       const char* data, int len) {
  return decode_binary_cell(static_cast<Oid>(type_oid), data, len);
}

}  // namespace relx::connection::sql_utils