#pragma once

#include "../results/result.hpp"
#include "connection.hpp"
#include "postgresql_errors.hpp"

#include <expected>
#include <functional>
#include <iostream>
#include <optional>
#include <string>

namespace relx::connection {

/// @brief Types of streaming errors
enum class StreamingErrorType : std::uint8_t {
  ConnectionLost,         ///< Database connection was lost during streaming
  QueryExecutionFailed,   ///< Query execution failed
  ResultProcessingError,  ///< Error processing a specific row/result
  MemoryAllocationError,  ///< Memory allocation failed
  NetworkError,           ///< Network/socket error
  TimeoutError,           ///< Operation timed out
  CancelledError,         ///< Operation was cancelled
  UnknownError            ///< Unknown or unclassified error
};

/// @brief Streaming-specific error information
struct StreamingError {
  StreamingErrorType error_type = StreamingErrorType::UnknownError;
  std::string message;
  int error_code = 0;

  // PostgreSQL-specific error details (when available)
  std::optional<std::string> sql_state;
  std::optional<std::string> detail;
  std::optional<std::string> hint;

  // Context information
  std::optional<size_t> row_number;        ///< Row number where error occurred (if applicable)
  std::optional<std::string> column_name;  ///< Column name where error occurred (if applicable)

  /// @brief Check if this is a recoverable error
  /// @return True if the error might be temporary and retryable
  bool is_recoverable() const {
    switch (error_type) {
    case StreamingErrorType::NetworkError:
    case StreamingErrorType::TimeoutError:
    case StreamingErrorType::MemoryAllocationError:
    case StreamingErrorType::ConnectionLost:
      return true;  // Can reconnect
    case StreamingErrorType::QueryExecutionFailed:
    case StreamingErrorType::ResultProcessingError:
    case StreamingErrorType::CancelledError:
    case StreamingErrorType::UnknownError:
      return false;
    }
    return false;
  }

  /// @brief Get a formatted error message with all available details
  /// @return Comprehensive error message
  std::string formatted_message() const {
    std::string result = message;

    if (sql_state) {
      result += " (SQLSTATE: " + *sql_state + ")";
    }

    if (error_code != 0) {
      result += " [Error Code: " + std::to_string(error_code) + "]";
    }

    if (row_number) {
      result += " [Row: " + std::to_string(*row_number) + "]";
    }

    if (column_name) {
      result += " [Column: " + *column_name + "]";
    }

    if (detail) {
      result += "\nDetail: " + *detail;
    }

    if (hint) {
      result += "\nHint: " + *hint;
    }

    return result;
  }

  /// @brief Create a StreamingError from a PostgreSQL error
  /// @param pg_error The PostgreSQL error to convert
  /// @param error_type The type of streaming error
  /// @return StreamingError with PostgreSQL details
  static StreamingError from_postgresql_error(
      const PostgreSQLError& pg_error,
      StreamingErrorType error_type = StreamingErrorType::QueryExecutionFailed) {
    StreamingError result;
    result.error_type = error_type;
    result.message = pg_error.message;
    result.error_code = static_cast<int>(pg_error.error_code);

    if (!pg_error.sql_state.empty()) {
      result.sql_state = pg_error.sql_state;
    }

    if (!pg_error.detail.empty()) {
      result.detail = pg_error.detail;
    }

    if (!pg_error.hint.empty()) {
      result.hint = pg_error.hint;
    }

    return result;
  }

  /// @brief Create a StreamingError from a ConnectionError
  /// @param conn_error The connection error to convert
  /// @param error_type The type of streaming error
  /// @return StreamingError with connection details
  static StreamingError from_connection_error(
      const ConnectionError& conn_error,
      StreamingErrorType error_type = StreamingErrorType::ConnectionLost) {
    StreamingError result;
    result.error_type = error_type;
    result.message = conn_error.message;
    result.error_code = conn_error.error_code;
    return result;
  }

  /// @brief Create a StreamingError from a ResultError
  /// @param result_error The result error to convert
  /// @param row_num Optional row number where error occurred
  /// @param col_name Optional column name where error occurred
  /// @return StreamingError with result processing details
  static StreamingError from_result_error(const result::ResultError& result_error,
                                          std::optional<size_t> row_num = std::nullopt,
                                          std::optional<std::string> col_name = std::nullopt) {
    StreamingError result;
    result.error_type = StreamingErrorType::ResultProcessingError;
    result.message = result_error.message;
    result.row_number = row_num;
    result.column_name = std::move(col_name);
    return result;
  }
};

/// @brief Result type for streaming row operations
/// @details This represents either:
/// - A successful row (string data)
/// - End of stream (std::nullopt)
/// - An error (StreamingError)
using StreamingRowResult = std::expected<std::optional<std::string>, StreamingError>;

/// @brief Result type for streaming initialization and control operations
template <typename T = void>
using StreamingResult = std::expected<T, StreamingError>;

/// @brief Helper to check if a StreamingRowResult indicates end of stream
/// @param result The result to check
/// @return True if this represents normal end of stream
inline bool is_end_of_stream(const StreamingRowResult& result) {
  return result.has_value() && !result->has_value();
}

/// @brief Helper to check if a StreamingRowResult contains an error
/// @param result The result to check
/// @return True if this represents an error condition
inline bool is_error(const StreamingRowResult& result) {
  return !result.has_value();
}

/// @brief Helper to check if a StreamingRowResult contains valid row data
/// @param result The result to check
/// @return True if this contains valid row data
inline bool has_row_data(const StreamingRowResult& result) {
  return result.has_value() && result->has_value();
}

/// @brief Helper to extract row data from a StreamingRowResult
/// @param result The result to extract from
/// @return The row data, or throws if not available
inline const std::string& get_row_data(const StreamingRowResult& result) {
  if (!has_row_data(result)) {
    throw std::runtime_error("No row data available in StreamingRowResult");
  }
  return **result;
}

/// @brief Callback type for streaming error handlers
/// @param error The streaming error that occurred
/// @param context Additional context about where the error occurred
/// @return True to continue streaming (if possible), false to stop
using StreamingErrorHandler =
    std::function<bool(const StreamingError& error, const std::string& context)>;

/// @brief Default error handler that logs errors and continues if recoverable
/// @param error The streaming error
/// @param context Additional context
/// @return True if error is recoverable, false otherwise
inline bool default_streaming_error_handler(const StreamingError& error,
                                            const std::string& context) {
  std::cerr << "Streaming error in " << context << ": " << error.formatted_message() << '\n';
  return error.is_recoverable();
}

}  // namespace relx::connection