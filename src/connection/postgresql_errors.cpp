#include "relx/connection/postgresql_errors.hpp"

#include <regex>
#include <sstream>

namespace relx::connection {

PostgreSQLError PostgreSQLError::from_libpq(int pg_error_code, std::string_view error_msg) {
  PostgreSQLError error;
  error.message = std::string(error_msg);

  // Map libpq error code to our error code
  switch (pg_error_code) {
  case 0:  // CONNECTION_OK
    // Not an error
    break;
  case 1:  // CONNECTION_BAD
    error.error_code = PostgreSQLErrorCode::ConnectionFailed;
    break;
  case 2:  // CONNECTION_STARTED
  case 3:  // CONNECTION_MADE
  case 4:  // CONNECTION_AWAITING_RESPONSE
  case 5:  // CONNECTION_AUTH_OK
  case 6:  // CONNECTION_SETENV
  case 7:  // CONNECTION_SSL_STARTUP
  case 8:  // CONNECTION_NEEDED
    // Connection in progress, not an error
    break;
  default:
    error.error_code = PostgreSQLErrorCode::Unknown;
    break;
  }

  // Look for SQLSTATE in the message
  static const std::regex sqlstate_regex(R"(ERROR:\s+\w+:\s+\d+\s+(\w+))");
  std::smatch match;
  const std::string error_msg_str(error_msg);
  if (std::regex_search(error_msg_str, match, sqlstate_regex) && match.size() > 1) {
    error.sql_state = match[1].str();

    // Check if we have a mapping for this SQLSTATE
    auto it = sql_state_map.find(error.sql_state);
    if (it != sql_state_map.end()) {
      error.error_code = it->second;
    }
  }

  // Parse other error details
  static const std::regex detail_regex(R"(DETAIL:\s+(.*))");
  static const std::regex hint_regex(R"(HINT:\s+(.*))");
  static const std::regex constraint_regex(R"(constraint \"([^"]+)\")");
  static const std::regex table_regex(R"(table \"([^"]+)\")");
  static const std::regex column_regex(R"(column \"([^"]+)\")");

  if (std::regex_search(error_msg_str, match, detail_regex) && match.size() > 1) {
    error.detail = match[1].str();
  }

  if (std::regex_search(error_msg_str, match, hint_regex) && match.size() > 1) {
    error.hint = match[1].str();
  }

  if (std::regex_search(error_msg_str, match, constraint_regex) && match.size() > 1) {
    error.constraint_name = match[1].str();
  }

  if (std::regex_search(error_msg_str, match, table_regex) && match.size() > 1) {
    error.table_name = match[1].str();
  }

  if (std::regex_search(error_msg_str, match, column_regex) && match.size() > 1) {
    error.column_name = match[1].str();
  }

  return error;
}

PostgreSQLError PostgreSQLError::from_sql_state(std::string_view sql_state,
                                                std::string_view error_msg) {
  PostgreSQLError error;
  error.message = std::string(error_msg);
  error.sql_state = std::string(sql_state);

  // Look up the error code from the SQL state
  auto it = sql_state_map.find(std::string(sql_state));
  if (it != sql_state_map.end()) {
    error.error_code = it->second;
  } else {
    error.error_code = PostgreSQLErrorCode::Unknown;
  }

  return error;
}

std::string PostgreSQLError::formatted_message() const {
  std::ostringstream ss;
  ss << message;

  if (!detail.empty()) {
    ss << "\nDetail: " << detail;
  }

  if (!hint.empty()) {
    ss << "\nHint: " << hint;
  }

  if (!sql_state.empty()) {
    ss << "\nSQL State: " << sql_state;
  }

  if (!constraint_name.empty()) {
    ss << "\nConstraint: " << constraint_name;
  }

  if (!table_name.empty()) {
    ss << "\nTable: " << table_name;
  }

  if (!column_name.empty()) {
    ss << "\nColumn: " << column_name;
  }

  return ss.str();
}

}  // namespace relx::connection