#include "relx/connection/postgresql_statement.hpp"

#include <iostream>
#include <sstream>

#include <libpq-fe.h>

namespace relx::connection {

PostgreSQLStatement::PostgreSQLStatement(PostgreSQLConnection& connection, std::string name,
                                         std::string sql, int param_count)
    : connection_(connection), name_(std::move(name)), sql_(std::move(sql)),
      param_count_(param_count) {
  // The statement should already be prepared by the PostgreSQLConnection class
  // We just initialize the object here
}

PostgreSQLStatement::~PostgreSQLStatement() {
  if (is_valid_) {
    // Deallocate the prepared statement
    auto result = connection_.execute_raw("DEALLOCATE " + name_);
    if (!result) {
      std::print("Failed to deallocate statement: {}", result.error().message);
    }
  }
}

PostgreSQLStatement::PostgreSQLStatement(PostgreSQLStatement&& other) noexcept
    : connection_(other.connection_), name_(std::move(other.name_)), sql_(std::move(other.sql_)),
      param_count_(other.param_count_), is_valid_(other.is_valid_) {
  // Mark the moved-from object as invalid
  other.is_valid_ = false;
}

PostgreSQLStatement& PostgreSQLStatement::operator=(PostgreSQLStatement&& other) noexcept {
  if (this != &other) {
    // Clean up this object
    if (is_valid_) {
      auto result = connection_.execute_raw("DEALLOCATE " + name_);
      if (!result) {
        std::print("Failed to deallocate statement: {}", result.error().message);
      }
    }

    // Move data from other
    name_ = std::move(other.name_);
    sql_ = std::move(other.sql_);
    param_count_ = other.param_count_;
    is_valid_ = other.is_valid_;

    // Mark the moved-from object as invalid
    other.is_valid_ = false;
  }
  return *this;
}

ConnectionResult<result::ResultSet> PostgreSQLStatement::execute(
    const std::vector<std::string>& params) {
  if (!is_valid_) {
    return std::unexpected(ConnectionError{.message = "Statement is not valid", .error_code = -1});
  }

  if (params.size() != static_cast<size_t>(param_count_)) {
    return std::unexpected(
        ConnectionError{.message = "Parameter count mismatch", .error_code = -1});
  }

  // Construct the EXECUTE statement
  std::ostringstream execute_sql;
  execute_sql << "EXECUTE " << name_;

  if (!params.empty()) {
    execute_sql << "(";
    for (size_t i = 0; i < params.size(); ++i) {
      if (i > 0) {
        execute_sql << ", ";
      }

      // For PostgreSQL, we need to properly quote values
      // NULL value
      if (params[i] == "NULL") {
        execute_sql << "NULL";
      }
      // Numeric value
      else if (std::all_of(params[i].begin(), params[i].end(), [](char c) {
                 return std::isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
               })) {
        execute_sql << params[i];
      }
      // Boolean value
      else if (params[i] == "t" || params[i] == "f" || params[i] == "true" ||
               params[i] == "false") {
        execute_sql << params[i];
      }
      // String value - needs quoting
      else {
        execute_sql << "'" << escape_string(params[i]) << "'";
      }
    }
    execute_sql << ")";
  }

  // Execute the statement
  return connection_.execute_raw(execute_sql.str());
}

ConnectionResult<PGresult*> PostgreSQLStatement::execute_raw(const std::vector<std::string>& params,
                                                             int result_format) {
  if (!is_valid_) {
    return std::unexpected(ConnectionError{.message = "Statement is not valid", .error_code = -1});
  }

  if (params.size() != static_cast<size_t>(param_count_)) {
    return std::unexpected(
        ConnectionError{.message = "Parameter count mismatch", .error_code = -1});
  }

  // This is not directly exposed because it requires direct PGconn access
  // and should be implemented in the PostgreSQLConnection class
  return std::unexpected(
      ConnectionError{.message = "Direct PGconn access not available through statement interface",
                      .error_code = -1});
}

ConnectionResult<result::ResultSet> PostgreSQLStatement::process_result(PGresult* pg_result) {
  // This should be implemented using the same logic as in PostgreSQLConnection::execute_raw
  // Since we're using higher-level execution through the connection, we don't need this.
  return std::unexpected(ConnectionError{
      .message = "Direct result processing not needed in statement interface", .error_code = -1});
}

// Helper to escape string literals for PostgreSQL
std::string PostgreSQLStatement::escape_string(const std::string& str) {
  std::string result;
  result.reserve(str.size());

  for (char c : str) {
    if (c == '\'') {
      result.append("''");  // Double single quotes to escape them
    } else {
      result.push_back(c);
    }
  }

  return result;
}

}  // namespace relx::connection