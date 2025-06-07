#include "relx/connection/postgresql_streaming_source.hpp"

#include "relx/connection/sql_utils.hpp"

#include <sstream>

#include <libpq-fe.h>

namespace relx::connection {

PostgreSQLStreamingSource::PostgreSQLStreamingSource(PostgreSQLConnection& connection,
                                                     std::string sql,
                                                     std::vector<std::string> params)
    : connection_(connection), sql_(std::move(sql)), params_(std::move(params)), use_binary_(false),
      initialized_(false), finished_(false), convert_bytea_(false), query_active_(false) {}

PostgreSQLStreamingSource::PostgreSQLStreamingSource(PostgreSQLConnection& connection,
                                                     std::string sql,
                                                     std::vector<std::string> params,
                                                     std::vector<bool> is_binary)
    : connection_(connection), sql_(std::move(sql)), params_(std::move(params)),
      is_binary_(std::move(is_binary)), use_binary_(true), initialized_(false), finished_(false),
      convert_bytea_(true), query_active_(false) {}

PostgreSQLStreamingSource::~PostgreSQLStreamingSource() {
  cleanup();
}

PostgreSQLStreamingSource::PostgreSQLStreamingSource(PostgreSQLStreamingSource&& other) noexcept
    : connection_(other.connection_), sql_(std::move(other.sql_)),
      params_(std::move(other.params_)), is_binary_(std::move(other.is_binary_)),
      use_binary_(other.use_binary_), column_names_(std::move(other.column_names_)),
      is_bytea_column_(std::move(other.is_bytea_column_)), initialized_(other.initialized_),
      finished_(other.finished_), convert_bytea_(other.convert_bytea_),
      query_active_(other.query_active_), first_row_cached_(std::move(other.first_row_cached_)) {
  // Mark the other object as moved-from
  other.initialized_ = false;
  other.finished_ = true;
  other.query_active_ = false;
  other.first_row_cached_.reset();
}

PostgreSQLStreamingSource& PostgreSQLStreamingSource::operator=(
    PostgreSQLStreamingSource&& other) noexcept {
  if (this != &other) {
    cleanup();  // Clean up current state

    connection_ = std::move(other.connection_);
    sql_ = std::move(other.sql_);
    params_ = std::move(other.params_);
    is_binary_ = std::move(other.is_binary_);
    use_binary_ = other.use_binary_;
    column_names_ = std::move(other.column_names_);
    is_bytea_column_ = std::move(other.is_bytea_column_);
    initialized_ = other.initialized_;
    finished_ = other.finished_;
    convert_bytea_ = other.convert_bytea_;
    query_active_ = other.query_active_;
    first_row_cached_ = std::move(other.first_row_cached_);

    // Mark the other object as moved-from
    other.initialized_ = false;
    other.finished_ = true;
    other.query_active_ = false;
    other.first_row_cached_.reset();
  }
  return *this;
}

ConnectionResult<void> PostgreSQLStreamingSource::initialize() {
  if (initialized_) {
    return {};
  }

  auto result = start_query();
  if (!result) {
    return result;
  }

  initialized_ = true;
  return {};
}

std::optional<std::string> PostgreSQLStreamingSource::get_next_row() {
  if (!initialized_ || finished_) {
    return std::nullopt;
  }

  // If we have a cached first row, return it
  if (first_row_cached_) {
    auto row = *first_row_cached_;
    first_row_cached_.reset();
    return row;
  }

  // Get the next result using PQgetResult
  PGconn* pg_conn = connection_.get_pg_conn();
  if (!pg_conn) {
    finished_ = true;
    return std::nullopt;
  }

  PGresult* pg_result = PQgetResult(pg_conn);
  if (!pg_result) {
    // No more results
    finished_ = true;
    query_active_ = false;
    return std::nullopt;
  }

  // Check result status
  ExecStatusType status = PQresultStatus(pg_result);

  if (status == PGRES_SINGLE_TUPLE) {
    // We have a single row, format it
    auto row_data = format_row(pg_result);
    PQclear(pg_result);
    return row_data;
  } else if (status == PGRES_TUPLES_OK) {
    // End of results
    PQclear(pg_result);
    finished_ = true;
    query_active_ = false;
    return std::nullopt;
  } else {
    // Error condition
    PQclear(pg_result);
    finished_ = true;
    query_active_ = false;
    return std::nullopt;
  }
}

const std::vector<std::string>& PostgreSQLStreamingSource::get_column_names() const {
  return column_names_;
}

ConnectionResult<void> PostgreSQLStreamingSource::start_query() {
  if (!connection_.is_connected()) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  PGconn* pg_conn = connection_.get_pg_conn();
  if (!pg_conn) {
    return std::unexpected(ConnectionError{.message = "Invalid connection", .error_code = -1});
  }

  int result_code;

  if (params_.empty()) {
    // Execute without parameters
    result_code = PQsendQuery(pg_conn, sql_.c_str());
  } else {
    // Convert ? placeholders to $1, $2, etc.
    std::string pg_sql = connection_.convert_placeholders(sql_);

    if (use_binary_) {
      // Prepare parameter values, lengths, and formats for binary mode
      std::vector<const char*> param_values;
      std::vector<int> param_formats;
      std::vector<int> param_lengths;

      param_values.reserve(params_.size());
      param_formats.reserve(params_.size());
      param_lengths.reserve(params_.size());

      for (size_t i = 0; i < params_.size(); ++i) {
        param_values.push_back(params_[i].c_str());
        param_lengths.push_back(static_cast<int>(params_[i].size()));
        param_formats.push_back(is_binary_[i] ? 1 : 0);
      }

      result_code = PQsendQueryParams(pg_conn, pg_sql.c_str(), static_cast<int>(params_.size()),
                                      nullptr,  // parameter types
                                      param_values.data(), param_lengths.data(),
                                      param_formats.data(),
                                      0);  // result format (text)
    } else {
      // Prepare parameter values for text mode
      std::vector<const char*> param_values;
      param_values.reserve(params_.size());

      for (const auto& param : params_) {
        param_values.push_back(param.c_str());
      }

      result_code = PQsendQueryParams(pg_conn, pg_sql.c_str(), static_cast<int>(params_.size()),
                                      nullptr,  // parameter types
                                      param_values.data(),
                                      nullptr,  // parameter lengths (null-terminated strings)
                                      nullptr,  // parameter formats (all text)
                                      0);       // result format (text)
    }
  }

  if (result_code != 1) {
    return std::unexpected(
        ConnectionError{.message = std::string("Failed to send query: ") + PQerrorMessage(pg_conn),
                        .error_code = -1});
  }

  // Enable single-row mode for streaming
  if (PQsetSingleRowMode(pg_conn) != 1) {
    return std::unexpected(
        ConnectionError{.message = "Failed to enable single-row mode", .error_code = -1});
  }

  // Get the first result to extract column metadata
  PGresult* first_result = PQgetResult(pg_conn);
  if (!first_result) {
    return std::unexpected(ConnectionError{.message = "No result received", .error_code = -1});
  }

  ExecStatusType status = PQresultStatus(first_result);

  if (status == PGRES_SINGLE_TUPLE) {
    // Process column metadata from the first row
    process_column_metadata(first_result);

    // Cache the first row data so we can return it when get_next_row() is called
    first_row_cached_ = format_row(first_result);

    PQclear(first_result);
    query_active_ = true;

    return {};
  } else if (status == PGRES_TUPLES_OK) {
    // Empty result set
    process_column_metadata(first_result);
    PQclear(first_result);
    finished_ = true;
    return {};
  } else {
    // Error
    std::string error_msg = PQresultErrorMessage(first_result);
    PQclear(first_result);
    return std::unexpected(
        ConnectionError{.message = std::string("Query execution failed: ") + error_msg,
                        .error_code = static_cast<int>(status)});
  }
}

void PostgreSQLStreamingSource::process_column_metadata(PGresult* pg_result) {
  int column_count = PQnfields(pg_result);

  column_names_.clear();
  column_names_.reserve(column_count);
  is_bytea_column_.clear();
  is_bytea_column_.reserve(column_count);

  for (int i = 0; i < column_count; i++) {
    const char* name = PQfname(pg_result, i);
    column_names_.push_back(name ? name : "");

    // Check if this is a BYTEA column (OID 17)
    is_bytea_column_.push_back(convert_bytea_ && (PQftype(pg_result, i) == 17));
  }
}

std::optional<std::string> PostgreSQLStreamingSource::format_row(PGresult* pg_result) {
  int column_count = PQnfields(pg_result);

  if (column_count == 0) {
    return std::nullopt;
  }

  std::ostringstream row_stream;

  for (int col_idx = 0; col_idx < column_count; col_idx++) {
    if (col_idx > 0) {
      row_stream << "|";
    }

    if (PQgetisnull(pg_result, 0, col_idx)) {
      row_stream << "NULL";
    } else {
      const char* value = PQgetvalue(pg_result, 0, col_idx);
      std::string cell_value = value ? value : "";

      // Convert BYTEA data from hex to binary if needed
      if (col_idx < static_cast<int>(is_bytea_column_.size()) && is_bytea_column_[col_idx]) {
        cell_value = convert_pg_bytea_to_binary(cell_value);
      }

      row_stream << cell_value;
    }
  }

  return row_stream.str();
}

std::string PostgreSQLStreamingSource::convert_pg_bytea_to_binary(
    const std::string& hex_value) const {
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

void PostgreSQLStreamingSource::cleanup() {
  if (query_active_) {
    // Consume any remaining results to clean up the connection state
    PGconn* pg_conn = connection_.get_pg_conn();
    if (pg_conn) {
      PGresult* result;
      while ((result = PQgetResult(pg_conn)) != nullptr) {
        PQclear(result);
      }
    }
    query_active_ = false;
  }
  finished_ = true;
}

}  // namespace relx::connection