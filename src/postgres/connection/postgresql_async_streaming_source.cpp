#include "relx/connection/postgresql_async_streaming_source.hpp"

#include "relx/connection/sql_utils.hpp"

#include <sstream>

#include <libpq-fe.h>

namespace relx::connection {

PostgreSQLAsyncStreamingSource::PostgreSQLAsyncStreamingSource(
    PostgreSQLAsyncConnection& connection, std::string sql, std::vector<std::string> params)
    : connection_(connection), sql_(std::move(sql)), params_(std::move(params)),
      initialized_(false), finished_(false), convert_bytea_(false), query_active_(false),
      current_result_(nullptr, PQclear), current_row_index_(0), has_pending_results_(false) {}

PostgreSQLAsyncStreamingSource::~PostgreSQLAsyncStreamingSource() {
  cleanup();
}

PostgreSQLAsyncStreamingSource::PostgreSQLAsyncStreamingSource(
    PostgreSQLAsyncStreamingSource&& other) noexcept
    : connection_(other.connection_), sql_(std::move(other.sql_)),
      params_(std::move(other.params_)), column_names_(std::move(other.column_names_)),
      is_bytea_column_(std::move(other.is_bytea_column_)), initialized_(other.initialized_),
      finished_(other.finished_), convert_bytea_(other.convert_bytea_),
      query_active_(other.query_active_), first_row_cached_(std::move(other.first_row_cached_)),
      current_result_(std::move(other.current_result_)),
      current_row_index_(other.current_row_index_),
      has_pending_results_(other.has_pending_results_) {
  // Reset the moved-from object
  other.initialized_ = false;
  other.finished_ = true;
  other.query_active_ = false;
  other.current_row_index_ = 0;
  other.has_pending_results_ = false;
}

PostgreSQLAsyncStreamingSource& PostgreSQLAsyncStreamingSource::operator=(
    PostgreSQLAsyncStreamingSource&& other) noexcept {
  if (this != &other) {
    cleanup();

    // connection_ is a reference, it cannot be reassigned
    // connection_ = other.connection_;  // This line is removed
    sql_ = std::move(other.sql_);
    params_ = std::move(other.params_);
    column_names_ = std::move(other.column_names_);
    is_bytea_column_ = std::move(other.is_bytea_column_);
    initialized_ = other.initialized_;
    finished_ = other.finished_;
    convert_bytea_ = other.convert_bytea_;
    query_active_ = other.query_active_;
    first_row_cached_ = std::move(other.first_row_cached_);
    current_result_ = std::move(other.current_result_);
    current_row_index_ = other.current_row_index_;
    has_pending_results_ = other.has_pending_results_;

    // Reset the moved-from object
    other.initialized_ = false;
    other.finished_ = true;
    other.query_active_ = false;
    other.current_row_index_ = 0;
    other.has_pending_results_ = false;
  }
  return *this;
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncStreamingSource::initialize() {
  if (initialized_) {
    co_return ConnectionResult<void>{};
  }

  auto result = co_await start_query();
  if (!result) {
    co_return std::unexpected(result.error());
  }

  initialized_ = true;
  co_return ConnectionResult<void>{};
}

boost::asio::awaitable<std::optional<std::string>> PostgreSQLAsyncStreamingSource::get_next_row() {
  if (!initialized_) {
    auto init_result = co_await initialize();
    if (!init_result) {
      co_return std::nullopt;
    }
  }

  if (finished_) {
    co_return std::nullopt;
  }

  // If we have a first row cached, return it
  if (first_row_cached_) {
    auto row = std::move(*first_row_cached_);
    first_row_cached_.reset();
    co_return row;
  }

  // Get the underlying PGconn from the async connection
  PGconn* pg_conn = connection_.get_async_conn().native_handle();
  if (!pg_conn) {
    finished_ = true;
    co_return std::nullopt;
  }

  try {
    // Wait for the next result asynchronously
    while (true) {
      // Check if we can consume input without blocking
      if (PQconsumeInput(pg_conn) == 0) {
        finished_ = true;
        co_return std::nullopt;
      }

      // Check if we can get a result without blocking
      if (!PQisBusy(pg_conn)) {
        PGresult* pg_result = PQgetResult(pg_conn);
        if (!pg_result) {
          // No more results
          finished_ = true;
          query_active_ = false;
          co_return std::nullopt;
        }

        // Handle the result
        ExecStatusType status = PQresultStatus(pg_result);

        if (status == PGRES_SINGLE_TUPLE) {
          // We have a single row, format it
          auto row_data = format_single_row(pg_result);
          PQclear(pg_result);
          co_return row_data;
        } else if (status == PGRES_TUPLES_OK) {
          // End of results
          PQclear(pg_result);
          finished_ = true;
          query_active_ = false;
          co_return std::nullopt;
        } else {
          // Error condition
          PQclear(pg_result);
          finished_ = true;
          query_active_ = false;
          co_return std::nullopt;
        }
      }

      // Still busy, wait for the socket to be readable
      boost::system::error_code ec;
      co_await connection_.get_async_conn().socket().async_wait(
          boost::asio::ip::tcp::socket::wait_read,
          boost::asio::redirect_error(boost::asio::use_awaitable, ec));

      if (ec) {
        finished_ = true;
        co_return std::nullopt;
      }
    }

  } catch (const std::exception& e) {
    finished_ = true;
    co_return std::nullopt;
  }
}

const std::vector<std::string>& PostgreSQLAsyncStreamingSource::get_column_names() const {
  return column_names_;
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncStreamingSource::start_query() {
  if (!connection_.is_connected()) {
    co_return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  PGconn* pg_conn = connection_.get_async_conn().native_handle();
  if (!pg_conn) {
    co_return std::unexpected(ConnectionError{.message = "Invalid connection", .error_code = -1});
  }

  try {
    int result_code;

    if (params_.empty()) {
      // Execute without parameters
      result_code = PQsendQuery(pg_conn, sql_.c_str());
    } else {
      // Convert ? placeholders to $1, $2, etc.
      std::string pg_sql = sql_utils::convert_placeholders_to_postgresql(sql_);

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

    if (result_code != 1) {
      co_return std::unexpected(ConnectionError{.message = std::string("Failed to send query: ") +
                                                           PQerrorMessage(pg_conn),
                                                .error_code = -1});
    }

    // Enable single-row mode for streaming
    if (PQsetSingleRowMode(pg_conn) != 1) {
      co_return std::unexpected(
          ConnectionError{.message = "Failed to enable single-row mode", .error_code = -1});
    }

    // Wait for the first result to extract column metadata asynchronously
    while (true) {
      // Check if we can consume input without blocking
      if (PQconsumeInput(pg_conn) == 0) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Failed to consume input: ") + PQerrorMessage(pg_conn),
            .error_code = -1});
      }

      // Check if we can get a result without blocking
      if (!PQisBusy(pg_conn)) {
        PGresult* first_result = PQgetResult(pg_conn);
        if (!first_result) {
          co_return std::unexpected(
              ConnectionError{.message = "No result received", .error_code = -1});
        }

        ExecStatusType status = PQresultStatus(first_result);

        if (status == PGRES_SINGLE_TUPLE) {
          // Process column metadata from the first row
          process_column_metadata_from_pg_result(first_result);

          // Cache the first row data so we can return it when get_next_row() is called
          first_row_cached_ = format_single_row(first_result);

          PQclear(first_result);
          query_active_ = true;

          co_return ConnectionResult<void>{};
        } else if (status == PGRES_TUPLES_OK) {
          // Empty result set - still process metadata if available
          if (PQnfields(first_result) > 0) {
            process_column_metadata_from_pg_result(first_result);
          }
          PQclear(first_result);
          finished_ = true;
          co_return ConnectionResult<void>{};
        } else {
          // Error
          std::string error_msg = PQresultErrorMessage(first_result);
          PQclear(first_result);
          co_return std::unexpected(
              ConnectionError{.message = std::string("Query execution failed: ") + error_msg,
                              .error_code = static_cast<int>(status)});
        }
      }

      // Still busy, wait for the socket to be readable
      boost::system::error_code ec;
      co_await connection_.get_async_conn().socket().async_wait(
          boost::asio::ip::tcp::socket::wait_read,
          boost::asio::redirect_error(boost::asio::use_awaitable, ec));

      if (ec) {
        co_return std::unexpected(
            ConnectionError{.message = ec.message(), .error_code = ec.value()});
      }
    }

  } catch (const std::exception& e) {
    co_return std::unexpected(ConnectionError{
        .message = std::string("Failed to start streaming query: ") + e.what(), .error_code = -1});
  }
}

void PostgreSQLAsyncStreamingSource::process_column_metadata_from_pg_result(PGresult* pg_result) {
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

std::string PostgreSQLAsyncStreamingSource::format_single_row(PGresult* pg_result) {
  if (!pg_result || PQntuples(pg_result) == 0) {
    return "";
  }

  int column_count = PQnfields(pg_result);
  std::ostringstream oss;

  for (int col = 0; col < column_count; ++col) {
    if (col > 0) {
      oss << "|";
    }

    if (PQgetisnull(pg_result, 0, col)) {
      oss << "NULL";
    } else {
      const char* value = PQgetvalue(pg_result, 0, col);
      std::string str_value = value ? value : "";

      // Convert bytea if needed
      if (col < static_cast<int>(is_bytea_column_.size()) && is_bytea_column_[col] &&
          convert_bytea_) {
        str_value = convert_pg_bytea_to_binary(str_value);
      }

      oss << str_value;
    }
  }

  return oss.str();
}

std::string PostgreSQLAsyncStreamingSource::convert_pg_bytea_to_binary(
    const std::string& hex_value) const {
  // PostgreSQL returns bytea in hex format like: \x48656c6c6f
  if (hex_value.length() < 2 || hex_value.substr(0, 2) != "\\x") {
    return hex_value;  // Not hex format, return as-is
  }

  std::string binary_result;
  const std::string hex_part = hex_value.substr(2);  // Skip \x prefix

  for (size_t i = 0; i < hex_part.length(); i += 2) {
    if (i + 1 < hex_part.length()) {
      std::string byte_str = hex_part.substr(i, 2);
      char byte = static_cast<char>(std::stoi(byte_str, nullptr, 16));
      binary_result.push_back(byte);
    }
  }

  return binary_result;
}

void PostgreSQLAsyncStreamingSource::cleanup() {
  if (query_active_) {
    // Consume any remaining results to clean up the connection state
    PGconn* pg_conn = connection_.get_async_conn().native_handle();
    if (pg_conn) {
      PGresult* result;
      while ((result = PQgetResult(pg_conn)) != nullptr) {
        PQclear(result);
      }
    }
    query_active_ = false;
  }

  current_result_.reset();
  current_row_index_ = 0;
  has_pending_results_ = false;
  finished_ = true;
}

boost::asio::awaitable<void> PostgreSQLAsyncStreamingSource::async_cleanup() {
  if (!query_active_) {
    co_return;
  }

  PGconn* pg_conn = connection_.get_async_conn().native_handle();
  if (!pg_conn) {
    query_active_ = false;
    finished_ = true;
    co_return;
  }

  try {
    // Asynchronously consume any remaining results to clean up the connection state
    while (query_active_) {
      // Check if we can consume input without blocking
      if (PQconsumeInput(pg_conn) == 0) {
        // Error consuming input, just clean up and exit
        break;
      }

      // Check if we can get a result without blocking
      if (!PQisBusy(pg_conn)) {
        PGresult* result = PQgetResult(pg_conn);
        if (!result) {
          // No more results - cleanup is complete
          break;
        }
        PQclear(result);
      } else {
        // Still busy, wait for the socket to be readable
        boost::system::error_code ec;
        co_await connection_.get_async_conn().socket().async_wait(
            boost::asio::ip::tcp::socket::wait_read,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec) {
          // Error waiting, just break out
          break;
        }
      }
    }
  } catch (...) {
    // Any exception during cleanup - just mark as finished
  }

  // Mark cleanup as complete
  query_active_ = false;
  current_result_.reset();
  current_row_index_ = 0;
  has_pending_results_ = false;
  finished_ = true;
}

}  // namespace relx::connection