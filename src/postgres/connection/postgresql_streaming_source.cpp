#include "relx/connection/postgresql_streaming_source.hpp"

#include "relx/connection/sql_utils.hpp"

#include <sstream>

#include <libpq-fe.h>

namespace relx::connection {

PostgreSQLStreamingSource::PostgreSQLStreamingSource(PostgreSQLConnection& connection,
                                                     std::string sql,
                                                     std::vector<bind_param> params)
    : connection_(&connection), sql_(std::move(sql)), params_(std::move(params)),
      initialized_(false), finished_(false), convert_bytea_(false), query_active_(false) {}

PostgreSQLStreamingSource::~PostgreSQLStreamingSource() {
  cleanup();
}

PostgreSQLStreamingSource::PostgreSQLStreamingSource(PostgreSQLStreamingSource&& other) noexcept
    : connection_(other.connection_), sql_(std::move(other.sql_)),
      params_(std::move(other.params_)), column_names_(std::move(other.column_names_)),
      is_bytea_column_(std::move(other.is_bytea_column_)), initialized_(other.initialized_),
      finished_(other.finished_), convert_bytea_(other.convert_bytea_),
      query_active_(other.query_active_), first_row_cached_(std::move(other.first_row_cached_)),
      last_error_(std::move(other.last_error_)) {
  // Mark the other object as moved-from
  other.initialized_ = false;
  other.finished_ = true;
  other.query_active_ = false;
  other.first_row_cached_.reset();
  other.last_error_.reset();
}

PostgreSQLStreamingSource& PostgreSQLStreamingSource::operator=(
    PostgreSQLStreamingSource&& other) noexcept {
  if (this != &other) {
    cleanup();  // Clean up current state

    connection_ = other.connection_;
    sql_ = std::move(other.sql_);
    params_ = std::move(other.params_);
    column_names_ = std::move(other.column_names_);
    is_bytea_column_ = std::move(other.is_bytea_column_);
    initialized_ = other.initialized_;
    finished_ = other.finished_;
    convert_bytea_ = other.convert_bytea_;
    query_active_ = other.query_active_;
    first_row_cached_ = std::move(other.first_row_cached_);
    last_error_ = std::move(other.last_error_);

    // Mark the other object as moved-from
    other.initialized_ = false;
    other.finished_ = true;
    other.query_active_ = false;
    other.first_row_cached_.reset();
    other.last_error_.reset();
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
  if (!initialized_) {
    // The first row pull starts the query, as it does on the async source. Callers that
    // reach the source through create_streaming_result never hold it directly and so have
    // no chance to call initialize() themselves.
    if (auto init_result = initialize(); !init_result) {
      last_error_ = init_result.error();
      finished_ = true;
      return std::nullopt;
    }
  }

  if (finished_) {
    return std::nullopt;
  }

  // If we have a cached first row, return it
  if (first_row_cached_) {
    auto row = *first_row_cached_;
    first_row_cached_.reset();
    return row;
  }

  // Get the next result using PQgetResult
  PGconn* pg_conn = connection_->get_pg_conn();
  if (!pg_conn) {
    last_error_ = ConnectionError{.message = "Invalid connection", .error_code = -1};
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
    try {
      auto row_data = format_row(pg_result);
      PQclear(pg_result);
      return row_data;
    } catch (const std::exception& e) {
      PQclear(pg_result);
      drain_results();
      last_error_ = ConnectionError{.message = std::string("Failed to decode row: ") + e.what(),
                                    .error_code = -1};
      finished_ = true;
      query_active_ = false;
      return std::nullopt;
    }
  } else if (status == PGRES_TUPLES_OK) {
    // End of results
    PQclear(pg_result);
    drain_results();
    finished_ = true;
    query_active_ = false;
    return std::nullopt;
  } else {
    // Error condition
    last_error_ = ConnectionError{.message = std::string("Streaming query failed: ") +
                                             PQresultErrorMessage(pg_result),
                                  .error_code = static_cast<int>(status)};
    PQclear(pg_result);
    drain_results();
    finished_ = true;
    query_active_ = false;
    return std::nullopt;
  }
}

const std::vector<std::string>& PostgreSQLStreamingSource::get_column_names() const {
  return column_names_;
}

ConnectionResult<void> PostgreSQLStreamingSource::start_query() {
  if (!connection_->is_connected()) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  PGconn* pg_conn = connection_->get_pg_conn();
  if (!pg_conn) {
    return std::unexpected(ConnectionError{.message = "Invalid connection", .error_code = -1});
  }

  int result_code;

  if (params_.empty()) {
    // Execute without parameters
    result_code = PQsendQuery(pg_conn, sql_.c_str());
  } else {
    // Convert ? placeholders to $1, $2, etc.
    std::string pg_sql = connection_->convert_placeholders(sql_);

    // Typed parameters: null params bind SQL NULL, kind-tagged params travel in
    // binary with their type OID, the rest as untyped text
    std::vector<Oid> types;
    std::vector<const char*> values;
    std::vector<int> lengths;
    std::vector<int> formats;
    types.reserve(params_.size());
    values.reserve(params_.size());
    lengths.reserve(params_.size());
    formats.reserve(params_.size());

    for (const auto& param : params_) {
      types.push_back(static_cast<Oid>(param.kind));
      if (param.is_null) {
        values.push_back(nullptr);  // SQL NULL
        lengths.push_back(0);
        formats.push_back(0);
      } else if (param.kind != relx::sql_kind::unspecified) {
        values.push_back(reinterpret_cast<const char*>(param.binary.data()));
        lengths.push_back(static_cast<int>(param.binary.size()));
        formats.push_back(1);  // binary
      } else {
        values.push_back(param.value.c_str());
        lengths.push_back(static_cast<int>(param.value.size()));
        formats.push_back(0);  // text
      }
    }

    result_code = PQsendQueryParams(pg_conn, pg_sql.c_str(), static_cast<int>(params_.size()),
                                    types.data(), values.data(), lengths.data(), formats.data(),
                                    0);  // result format (text)
  }

  if (result_code != 1) {
    return std::unexpected(
        ConnectionError{.message = std::string("Failed to send query: ") + PQerrorMessage(pg_conn),
                        .error_code = -1});
  }

  // Enable single-row mode for streaming
  if (PQsetSingleRowMode(pg_conn) != 1) {
    drain_results();  // the query is already in flight
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
    drain_results();
    finished_ = true;
    return {};
  } else {
    // Error
    std::string error_msg = PQresultErrorMessage(first_result);
    PQclear(first_result);
    drain_results();
    finished_ = true;
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
      row_stream << result::text_format::null_marker;
    } else {
      const char* value = PQgetvalue(pg_result, 0, col_idx);
      std::string cell_value = value ? value : "";

      // Convert BYTEA data from hex to binary if needed
      if (col_idx < static_cast<int>(is_bytea_column_.size()) && is_bytea_column_[col_idx]) {
        cell_value = convert_pg_bytea_to_binary(cell_value);
      }

      row_stream << result::text_format::escape(cell_value);
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
        const std::string hex_byte = hex_value.substr(i, 2);
        size_t consumed = 0;
        const int byte = std::stoi(hex_byte, &consumed, 16);
        if (consumed != 2) {
          throw std::invalid_argument("Invalid BYTEA hex digit in '" + hex_byte + "'");
        }
        binary_result.push_back(static_cast<char>(byte));
      }
    }

    return binary_result;
  }

  // If not in hex format, return as is
  return hex_value;
}

void PostgreSQLStreamingSource::cleanup() {
  if (query_active_) {
    drain_results();
    query_active_ = false;
  }
  finished_ = true;
}

void PostgreSQLStreamingSource::drain_results() {
  PGconn* pg_conn = connection_->get_pg_conn();
  if (!pg_conn) {
    return;
  }

  PGresult* result;
  while ((result = PQgetResult(pg_conn)) != nullptr) {
    PQclear(result);
  }
}

}  // namespace relx::connection