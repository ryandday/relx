#pragma once

#include <string>

// Forward declarations
namespace relx::result {
class ResultSet;
}

// Forward declare PostgreSQL types to avoid libpq header dependency
struct pg_result;
using PGresult = pg_result;

namespace relx::connection::sql_utils {

/// @brief Convert ? placeholders to PostgreSQL $1, $2, etc. format
/// @details This function properly handles SQL syntax by respecting:
/// - Single quotes (string literals): 'Don''t replace ? here'
/// - Double quotes (quoted identifiers): "weird?column"
/// - Escaped quotes: '' and ""
/// Only replaces ? that appear outside of quoted contexts.
/// @param sql The SQL string with ? placeholders
/// @return SQL string with $1, $2, etc. placeholders
std::string convert_placeholders_to_postgresql(const std::string& sql);

/// @brief Convert IsolationLevel enum to PostgreSQL isolation level string
/// @param isolation_level The isolation level enum value (cast from IsolationLevel)
/// @return PostgreSQL-compatible isolation level string
std::string isolation_level_to_postgresql_string(int isolation_level);

/// @brief Process PostgreSQL result into relx ResultSet format
/// @param pg_result Pointer to PGresult from libpq
/// @param convert_bytea Whether to convert BYTEA columns from hex to binary
/// @return ResultSet containing the processed data
result::ResultSet process_postgresql_result(PGresult* pg_result, bool convert_bytea = false);

}  // namespace relx::connection::sql_utils