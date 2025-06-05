#pragma once

#include <string>
#include <unordered_map>

namespace relx::connection {

/// @brief PostgreSQL specific error codes
enum class PostgreSQLErrorCode {
  // Connection errors
  ConnectionFailed = 1000,
  ConnectionClosed = 1001,
  ConnectionTimeout = 1002,

  // Transaction errors
  TransactionError = 2000,
  NoActiveTransaction = 2001,
  TransactionAlreadyActive = 2002,

  // Query errors
  QueryFailed = 3000,
  InvalidParameters = 3001,
  EmptyResult = 3002,

  // SQLSTATE errors
  DuplicateKey = 23505,              // unique_violation
  ForeignKeyViolation = 23503,       // foreign_key_violation
  CheckConstraintViolation = 23514,  // check_violation
  NotNullViolation = 23502,          // not_null_violation

  // Generic errors
  Unknown = 9999
};

/// @brief Specialized ConnectionError for PostgreSQL with detailed error information
struct PostgreSQLError {
  std::string message;
  PostgreSQLErrorCode error_code = PostgreSQLErrorCode::Unknown;
  std::string sql_state;
  std::string constraint_name;
  std::string table_name;
  std::string column_name;
  std::string detail;
  std::string hint;

  /// @brief Create an error from a libpq error code and message
  /// @param pg_error_code The PostgreSQL error code
  /// @param error_msg The error message
  static PostgreSQLError from_libpq(int pg_error_code, std::string_view error_msg);

  /// @brief Create an error from a SQLSTATE code
  /// @param sql_state The SQLSTATE code
  /// @param error_msg The error message
  static PostgreSQLError from_sql_state(std::string_view sql_state, std::string_view error_msg);

  /// @brief Get a user-friendly error message
  /// @return A formatted error message with details
  std::string formatted_message() const;

  /// @brief Check if this is a duplicate key error
  /// @return True if this is a duplicate key error
  bool is_duplicate_key_error() const { return error_code == PostgreSQLErrorCode::DuplicateKey; }

  /// @brief Check if this is a foreign key violation
  /// @return True if this is a foreign key violation
  bool is_foreign_key_violation() const {
    return error_code == PostgreSQLErrorCode::ForeignKeyViolation;
  }

  /// @brief Check if this is a check constraint violation
  /// @return True if this is a check constraint violation
  bool is_check_constraint_violation() const {
    return error_code == PostgreSQLErrorCode::CheckConstraintViolation;
  }

  /// @brief Check if this is a not-null violation
  /// @return True if this is a not-null violation
  bool is_not_null_violation() const { return error_code == PostgreSQLErrorCode::NotNullViolation; }
};

/// @brief Map of SQL STATE codes to PostgreSQLErrorCode values
inline const std::unordered_map<std::string, PostgreSQLErrorCode> SQL_STATE_MAP = {
    // Class 23 — Integrity Constraint Violation
    {"23505", PostgreSQLErrorCode::DuplicateKey},
    {"23503", PostgreSQLErrorCode::ForeignKeyViolation},
    {"23514", PostgreSQLErrorCode::CheckConstraintViolation},
    {"23502", PostgreSQLErrorCode::NotNullViolation},

    // Add more mappings as needed
};

}  // namespace relx::connection