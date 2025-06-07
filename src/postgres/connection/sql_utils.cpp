#include "relx/connection/sql_utils.hpp"

#include "relx/results.hpp"

#include <libpq-fe.h>

namespace relx::connection::sql_utils {

std::string convert_placeholders_to_postgresql(const std::string& sql) {
  std::string result;
  result.reserve(sql.size() + 32);  // Reserve some extra space for parameter numbers

  int placeholder_count = 1;
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  for (size_t i = 0; i < sql.size(); ++i) {
    const char current = sql[i];

    // Handle single quotes (string literals)
    if (current == '\'' && !in_double_quotes) {
      // Check if this is an escaped quote (two single quotes in a row)
      if (i + 1 < sql.size() && sql[i + 1] == '\'') {
        // This is an escaped quote, add both characters and skip the next one
        result += current;
        result += sql[++i];
        continue;
      } else {
        // This is a regular single quote, toggle the state
        in_single_quotes = !in_single_quotes;
      }
    }
    // Handle double quotes (quoted identifiers)
    else if (current == '"' && !in_single_quotes) {
      // Check if this is an escaped quote (two double quotes in a row)
      if (i + 1 < sql.size() && sql[i + 1] == '"') {
        // This is an escaped quote, add both characters and skip the next one
        result += current;
        result += sql[++i];
        continue;
      } else {
        // This is a regular double quote, toggle the state
        in_double_quotes = !in_double_quotes;
      }
    }
    // Handle question marks (parameter placeholders)
    else if (current == '?' && !in_single_quotes && !in_double_quotes) {
      // This is a parameter placeholder outside of quotes, replace it
      result += '$';
      result += std::to_string(placeholder_count++);
      continue;
    }

    // For all other characters, just add them to the result
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

// Helper function to convert PostgreSQL hex BYTEA format to binary
static std::string convert_pg_bytea_to_binary(const std::string& hex_value) {
  // Check if this is a PostgreSQL hex-encoded BYTEA value (starts with \x)
  if (hex_value.size() >= 2 && hex_value.substr(0, 2) == "\\x") {
    std::string binary_result;
    binary_result.reserve((hex_value.size() - 2) / 2);

    // Skip the \x prefix and process each hex byte
    for (size_t i = 2; i < hex_value.size(); i += 2) {
      if (i + 1 < hex_value.size()) {
        try {
          const std::string hex_byte = hex_value.substr(i, 2);
          const char byte = static_cast<char>(std::stoi(hex_byte, nullptr, 16));
          binary_result.push_back(byte);
        } catch (const std::exception&) {
          // If conversion fails, just return the original value
          return hex_value;
        }
      }
    }

    return binary_result;
  }

  // If not in hex format, return as is
  return hex_value;
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
        cells.emplace_back("NULL");
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

}  // namespace relx::connection::sql_utils