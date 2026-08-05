#pragma once

#include <expected>
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

/// @brief Process a PostgreSQL result whose cells arrived in binary format
/// (resultFormat=1), decoding each cell into the canonical text form the rest of the
/// stack expects. Decoding is driven by the column type OID: bool/int/float wire
/// encodings are decoded, text-family and bytea pass through raw, user-defined types
/// (OID >= 16384, i.e. enums) pass through as their label text. An OID outside that set
/// is an error naming the column - callers gate binary results on the select list, so
/// this only fires when a struct field type does not match the actual column type.
/// @param pg_result Pointer to PGresult from libpq (binary result format)
/// @return ResultSet, or an error message describing the undecodable column
std::expected<result::ResultSet, std::string> process_postgresql_result_binary(PGresult* pg_result);

/// @brief Decode one binary-format cell into canonical text (testing/fuzzing hook for
/// the wire decoder behind process_postgresql_result_binary)
/// @param type_oid The column's PostgreSQL type OID
/// @param data The raw cell bytes
/// @param len The cell length in bytes
/// @return The text form, or an error message
std::expected<std::string, std::string> decode_binary_cell_for_testing(unsigned int type_oid,
                                                                       const char* data, int len);

}  // namespace relx::connection::sql_utils