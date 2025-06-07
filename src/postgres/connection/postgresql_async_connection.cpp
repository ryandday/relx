#include "relx/connection/postgresql_async_connection.hpp"

#include "relx/connection/sql_utils.hpp"

#include <iostream>
#include <regex>

namespace relx::connection {

PostgreSQLAsyncConnection::PostgreSQLAsyncConnection(boost::asio::io_context& io_context,
                                                     std::string connection_string)
    : io_context_(io_context), connection_string_(std::move(connection_string)),
      async_conn_(std::make_unique<pgsql_async_wrapper::Connection>(io_context)) {}

PostgreSQLAsyncConnection::PostgreSQLAsyncConnection(boost::asio::io_context& io_context,
                                                     const PostgreSQLConnectionParams& params)
    : io_context_(io_context), connection_string_(params.to_connection_string()),
      async_conn_(std::make_unique<pgsql_async_wrapper::Connection>(io_context)) {}

PostgreSQLAsyncConnection::~PostgreSQLAsyncConnection() {
  // Automatically disconnect if still connected
  if (is_connected()) {
    [[maybe_unused]] auto _ = disconnect();
  }
}

PostgreSQLAsyncConnection::PostgreSQLAsyncConnection(PostgreSQLAsyncConnection&& other) noexcept
    : io_context_(other.io_context_), connection_string_(std::move(other.connection_string_)),
      async_conn_(std::move(other.async_conn_)), is_connected_(other.is_connected_) {
  other.is_connected_ = false;
}

PostgreSQLAsyncConnection& PostgreSQLAsyncConnection::operator=(
    PostgreSQLAsyncConnection&& other) noexcept {
  if (this != &other) {
    // Disconnect first if connected
    if (is_connected()) {
      [[maybe_unused]] auto _ = disconnect();
    }

    connection_string_ = std::move(other.connection_string_);
    async_conn_ = std::move(other.async_conn_);
    is_connected_ = other.is_connected_;

    other.is_connected_ = false;
  }
  return *this;
}

bool PostgreSQLAsyncConnection::is_connected() const {
  return is_connected_ && async_conn_ && async_conn_->is_open();
}

bool PostgreSQLAsyncConnection::in_transaction() const {
  return is_connected() && async_conn_->in_transaction();
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::connect() {
  if (is_connected()) {
    co_return ConnectionResult<void>{};  // Already connected
  }

  // Make sure the connection object exists
  if (!async_conn_) {
    async_conn_ = std::make_unique<pgsql_async_wrapper::Connection>(io_context_);
  }

  // Connect with a copy of the connection string
  const std::string conn_str_copy = connection_string_;
  auto connect_result = co_await async_conn_->connect(conn_str_copy);

  if (!connect_result) {
    co_return std::unexpected(ConnectionError{.message = "Failed to connect to database: " +
                                                         connect_result.error().message,
                                              .error_code = connect_result.error().error_code});
  }

  is_connected_ = true;
  co_return ConnectionResult<void>{};
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::disconnect() {
  if (!is_connected()) {
    co_return ConnectionResult<void>{};  // Already disconnected
  }

  if (async_conn_) {
    async_conn_->close();
  }

  is_connected_ = false;
  co_return ConnectionResult<void>{};
}

boost::asio::awaitable<ConnectionResult<result::ResultSet>> PostgreSQLAsyncConnection::execute_raw(
    std::string sql, std::vector<std::string> params) {
  if (!is_connected()) {
    co_return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  // Convert ? placeholders to $1, $2, etc. if params exist
  if (!params.empty()) {
    sql = convert_placeholders(sql);
  }

  // Execute the query with copies of the parameters
  const std::string sql_copy = sql;
  const std::vector<std::string> params_copy = params;

  auto pg_result = co_await async_conn_->query(sql_copy, params_copy);

  if (!pg_result) {
    co_return std::unexpected(
        ConnectionError{.message = "Query execution failed: " + pg_result.error().message,
                        .error_code = pg_result.error().error_code});
  }

  // Convert the result to our ResultSet type
  auto result_set = convert_result(*pg_result);
  if (!result_set) {
    co_return std::unexpected(result_set.error());
  }

  co_return *result_set;
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::begin_transaction(
    IsolationLevel isolation_level) {
  if (!is_connected()) {
    co_return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  pgsql_async_wrapper::IsolationLevel pg_isolation;

  // Convert our isolation level to pgsql_async_wrapper's isolation level
  switch (isolation_level) {
  case IsolationLevel::ReadUncommitted:
    pg_isolation = pgsql_async_wrapper::IsolationLevel::ReadUncommitted;
    break;
  case IsolationLevel::ReadCommitted:
    pg_isolation = pgsql_async_wrapper::IsolationLevel::ReadCommitted;
    break;
  case IsolationLevel::RepeatableRead:
    pg_isolation = pgsql_async_wrapper::IsolationLevel::RepeatableRead;
    break;
  case IsolationLevel::Serializable:
    pg_isolation = pgsql_async_wrapper::IsolationLevel::Serializable;
    break;
  default:
    pg_isolation = pgsql_async_wrapper::IsolationLevel::ReadCommitted;
    break;
  }

  // Begin the transaction
  auto begin_result = co_await async_conn_->begin_transaction(pg_isolation);

  if (!begin_result) {
    co_return std::unexpected(
        ConnectionError{.message = "Failed to begin transaction: " + begin_result.error().message,
                        .error_code = begin_result.error().error_code});
  }

  co_return ConnectionResult<void>{};
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::commit_transaction() {
  if (!is_connected()) {
    co_return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  if (!in_transaction()) {
    co_return std::unexpected(
        ConnectionError{.message = "No active transaction", .error_code = -1});
  }

  auto commit_result = co_await async_conn_->commit();

  if (!commit_result) {
    co_return std::unexpected(
        ConnectionError{.message = "Failed to commit transaction: " + commit_result.error().message,
                        .error_code = commit_result.error().error_code});
  }

  co_return ConnectionResult<void>{};
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::rollback_transaction() {
  if (!is_connected()) {
    co_return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  if (!in_transaction()) {
    co_return std::unexpected(
        ConnectionError{.message = "No active transaction", .error_code = -1});
  }

  auto rollback_result = co_await async_conn_->rollback();

  if (!rollback_result) {
    co_return std::unexpected(ConnectionError{.message = "Failed to rollback transaction: " +
                                                         rollback_result.error().message,
                                              .error_code = rollback_result.error().error_code});
  }

  co_return ConnectionResult<void>{};
}

boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::reset_connection_state() {
  if (!is_connected()) {
    co_return ConnectionResult<void>{};  // Nothing to reset if not connected
  }

  if (!async_conn_) {
    co_return ConnectionResult<void>{};
  }

  PGconn* pg_conn = async_conn_->native_handle();
  if (!pg_conn) {
    co_return ConnectionResult<void>{};
  }

  try {
    // Consume any remaining results from the connection to clean up the state
    while (true) {
      // Check if we can consume input without blocking
      if (PQconsumeInput(pg_conn) == 0) {
        // Error consuming input - but continue to try to reset
        break;
      }

      // Check if we can get a result without blocking
      if (!PQisBusy(pg_conn)) {
        PGresult* result = PQgetResult(pg_conn);
        if (!result) {
          // No more results - connection is clean
          break;
        }
        PQclear(result);
      } else {
        // Still busy, wait for the socket to be readable
        boost::system::error_code ec;
        co_await async_conn_->socket().async_wait(
            boost::asio::ip::tcp::socket::wait_read,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec) {
          // Error waiting - connection might be in a bad state, but we'll return success anyway
          break;
        }
      }
    }
  } catch (...) {
    // Any exception during reset - just continue, connection might be reset anyway
  }

  co_return ConnectionResult<void>{};
}

std::string PostgreSQLAsyncConnection::convert_placeholders(const std::string& sql) {
  return sql_utils::convert_placeholders_to_postgresql(sql);
}

ConnectionResult<result::ResultSet> PostgreSQLAsyncConnection::convert_result(
    const pgsql_async_wrapper::Result& pg_result) {
  if (!pg_result.ok()) {
    return std::unexpected(
        ConnectionError{.message = "PostgreSQL error: " + std::string(pg_result.error_message()),
                        .error_code = static_cast<int>(pg_result.status())});
  }

  // Get column names
  std::vector<std::string> column_names;
  const int col_count = pg_result.columns();
  column_names.reserve(col_count);

  for (int i = 0; i < col_count; ++i) {
    const char* name = pg_result.field_name(i);
    column_names.push_back(name ? name : "");
  }

  // Create rows
  std::vector<result::Row> rows;
  const int row_count = pg_result.rows();
  rows.reserve(row_count);

  for (int r = 0; r < row_count; ++r) {
    std::vector<result::Cell> cells;
    cells.reserve(col_count);

    for (int c = 0; c < col_count; ++c) {
      if (pg_result.is_null(r, c)) {
        cells.emplace_back("NULL");
      } else {
        const char* value = pg_result.get_value(r, c);
        cells.emplace_back(value ? value : "");
      }
    }

    rows.emplace_back(std::move(cells), column_names);
  }

  return result::ResultSet(std::move(rows), std::move(column_names));
}

}  // namespace relx::connection