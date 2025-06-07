// Asynchronous PostgreSQL Client with C++20 Coroutines
// This library wraps libpq's asynchronous API with C++20 coroutines and Boost.Asio
// to provide a clean, modern interface for PostgreSQL database operations.

#pragma once

#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <libpq-fe.h>

namespace relx::pgsql_async_wrapper {

// Error types
struct PgError {
  std::string message;
  int error_code = 0;

  static PgError from_conn(PGconn* conn) {
    return PgError{.message = PQerrorMessage(conn) ? PQerrorMessage(conn)
                                                   : "Unknown PostgreSQL error",
                   .error_code = PQstatus(conn)};
  }

  static PgError from_result(PGresult* result) {
    return PgError{.message = PQresultErrorMessage(result) ? PQresultErrorMessage(result)
                                                           : "Unknown PostgreSQL error",
                   .error_code = static_cast<int>(PQresultStatus(result))};
  }
};

/**
 * @brief Format a PgError for exception messages
 */
inline std::string format_error(const PgError& error) {
  return std::format("PostgreSQL error: {} (Code: {})", error.message, error.error_code);
}

// Type alias for result of operations
template <typename T>
using PgResult = std::expected<T, PgError>;

// Result class to handle PGresult
class Result {
private:
  PGresult* res_ = nullptr;

public:
  Result() = default;

  explicit Result(PGresult* res) : res_(res) {}

  ~Result() { clear(); }

  Result(const Result&) = delete;
  Result& operator=(const Result&) = delete;

  Result(Result&& other) noexcept : res_(other.res_) { other.res_ = nullptr; }

  Result& operator=(Result&& other) noexcept {
    if (this != &other) {
      clear();
      res_ = other.res_;
      other.res_ = nullptr;
    }
    return *this;
  }

  void clear() {
    if (res_) {
      PQclear(res_);
      res_ = nullptr;
    }
  }

  bool ok() const {
    if (!res_) {
      return false;
    }
    auto status = PQresultStatus(res_);
    return status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
  }

  ExecStatusType status() const { return res_ ? PQresultStatus(res_) : PGRES_FATAL_ERROR; }

  const char* error_message() const {
    return res_ ? PQresultErrorMessage(res_) : "No result available";
  }

  int rows() const { return res_ ? PQntuples(res_) : 0; }

  int columns() const { return res_ ? PQnfields(res_) : 0; }

  const char* field_name(int col) const { return res_ ? PQfname(res_, col) : nullptr; }

  Oid field_type(int col) const { return res_ ? PQftype(res_, col) : 0; }

  int field_size(int col) const { return res_ ? PQfsize(res_, col) : 0; }

  int field_number(const char* name) const { return res_ ? PQfnumber(res_, name) : -1; }

  bool is_null(int row, int col) const { return res_ ? PQgetisnull(res_, row, col) != 0 : true; }

  const char* get_value(int row, int col) const {
    return res_ ? PQgetvalue(res_, row, col) : nullptr;
  }

  int get_length(int row, int col) const { return res_ ? PQgetlength(res_, row, col) : 0; }

  PGresult* get() const { return res_; }

  operator bool() const { return ok(); }
};

// Transaction isolation level
enum class IsolationLevel { ReadUncommitted, ReadCommitted, RepeatableRead, Serializable };

// Forward declaration
class Connection;

// Prepared statement class
class PreparedStatement {
private:
  Connection& conn_;
  std::string name_;
  std::string query_;
  bool prepared_ = false;

public:
  PreparedStatement(Connection& conn, std::string name, std::string query)
      : conn_(conn), name_(std::move(name)), query_(std::move(query)) {}

  ~PreparedStatement() = default;

  // Non-copyable
  PreparedStatement(const PreparedStatement&) = delete;
  PreparedStatement& operator=(const PreparedStatement&) = delete;

  // Move constructible/assignable
  PreparedStatement(PreparedStatement&& other) noexcept
      : conn_(other.conn_), name_(std::move(other.name_)), query_(std::move(other.query_)),
        prepared_(other.prepared_) {
    other.prepared_ = false;
  }

  PreparedStatement& operator=(PreparedStatement&& other) noexcept {
    if (this != &other) {
      name_ = std::move(other.name_);
      query_ = std::move(other.query_);
      prepared_ = other.prepared_;
      other.prepared_ = false;
    }
    return *this;
  }

  const std::string& name() const { return name_; }
  const std::string& query() const { return query_; }
  bool is_prepared() const { return prepared_; }

  // The implementation of prepare and execute will be defined in separate cpp file
  boost::asio::awaitable<PgResult<void>> prepare();
  boost::asio::awaitable<PgResult<Result>> execute(const std::vector<std::string>& params);
  boost::asio::awaitable<PgResult<void>> deallocate();

  friend class Connection;
};

// ----------------------------------------------------------------------
// The connection class - main interface for PostgreSQL operations
// ----------------------------------------------------------------------
class Connection {
private:
  boost::asio::io_context& io_;
  PGconn* conn_ = nullptr;
  std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
  std::unordered_map<std::string, std::shared_ptr<PreparedStatement>> statements_;
  bool in_transaction_ = false;

  PgResult<void> create_socket() {
    if (conn_ == nullptr) {
      return std::unexpected(
          PgError{.message = "Cannot create socket: no connection", .error_code = -1});
    }

    const int sock = PQsocket(conn_);
    if (sock < 0) {
      return std::unexpected(PgError{.message = "Invalid socket", .error_code = -1});
    }

    socket_ = std::make_unique<boost::asio::ip::tcp::socket>(io_, boost::asio::ip::tcp::v4(), sock);
    return PgResult<void>{};
  }

  // Asynchronous flush of outgoing data to the PostgreSQL server
  boost::asio::awaitable<PgResult<void>> flush_outgoing_data() {
    while (true) {
      const int flush_result = PQflush(conn_);
      if (flush_result == -1) {
        co_return std::unexpected(PgError::from_conn(conn_));
      }
      if (flush_result == 0) {
        break;  // All data has been flushed
      }
      // Flush returned 1, need to wait for socket to be writable
      boost::system::error_code ec;
      co_await socket().async_wait(boost::asio::ip::tcp::socket::wait_write,
                                   boost::asio::redirect_error(boost::asio::use_awaitable, ec));

      if (ec) {
        co_return std::unexpected(PgError{.message = ec.message(), .error_code = ec.value()});
      }
    }
    co_return PgResult<void>{};
  }

  // Helper method to wait for and get a query result
  boost::asio::awaitable<PgResult<Result>> get_query_result() {
    while (true) {
      // Check if we can consume input without blocking
      if (PQconsumeInput(conn_) == 0) {
        co_return std::unexpected(PgError::from_conn(conn_));
      }

      // Check if we can get a result without blocking
      if (!PQisBusy(conn_)) {
        PGresult* res = PQgetResult(conn_);
        Result result_obj(res);

        // Flush all remaining results (normally there should be none for a single query)
        while ((res = PQgetResult(conn_)) != nullptr) {
          PQclear(res);
        }

        co_return result_obj;
      }

      // Still busy, wait for the socket to be readable
      boost::system::error_code ec;
      co_await socket().async_wait(boost::asio::ip::tcp::socket::wait_read,
                                   boost::asio::redirect_error(boost::asio::use_awaitable, ec));

      if (ec) {
        co_return std::unexpected(PgError{.message = ec.message(), .error_code = ec.value()});
      }
    }
  }

public:
  Connection(boost::asio::io_context& io) : io_(io) {}

  ~Connection() { close(); }

  // Non-copyable
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // Move constructible/assignable
  Connection(Connection&& other) noexcept
      : io_(other.io_), conn_(other.conn_), socket_(std::move(other.socket_)),
        statements_(std::move(other.statements_)), in_transaction_(other.in_transaction_) {
    other.conn_ = nullptr;
    other.in_transaction_ = false;
  }

  Connection& operator=(Connection&& other) noexcept {
    if (this != &other) {
      close();
      conn_ = other.conn_;
      socket_ = std::move(other.socket_);
      statements_ = std::move(other.statements_);
      in_transaction_ = other.in_transaction_;
      other.conn_ = nullptr;
      other.in_transaction_ = false;
    }
    return *this;
  }

  void close() {
    statements_.clear();

    if (socket_) {
      socket_->close();
      socket_.reset();
    }

    if (conn_) {
      PQfinish(conn_);
      conn_ = nullptr;
    }

    in_transaction_ = false;
  }

  bool is_open() const { return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK; }

  bool in_transaction() const { return in_transaction_; }

  PGconn* native_handle() { return conn_; }

  boost::asio::ip::tcp::socket& socket() {
    if (!socket_) {
      throw std::runtime_error("Socket not initialized");
    }
    return *socket_;
  }

  // Asynchronous connection using boost::asio::awaitable
  boost::asio::awaitable<PgResult<void>> connect(const std::string& conninfo) {
    if (conn_ != nullptr) {
      close();
    }

    // Start the connection process
    conn_ = PQconnectStart(conninfo.c_str());
    if (conn_ == nullptr) {
      co_return std::unexpected(PgError{.message = "Out of memory", .error_code = -1});
    }

    // Check if connection immediately failed
    if (PQstatus(conn_) == CONNECTION_BAD) {
      auto error = PgError::from_conn(conn_);
      close();
      co_return std::unexpected(error);
    }

    // Set the connection to non-blocking
    if (PQsetnonblocking(conn_, 1) != 0) {
      auto error = PgError::from_conn(conn_);
      close();
      co_return std::unexpected(error);
    }

    // Create the socket
    auto socket_result = create_socket();
    if (!socket_result) {
      close();
      co_return std::unexpected(socket_result.error());
    }

    // Connection polling loop
    while (true) {
      const PostgresPollingStatusType poll_status = PQconnectPoll(conn_);

      if (poll_status == PGRES_POLLING_FAILED) {
        auto error = PgError::from_conn(conn_);
        close();
        co_return std::unexpected(error);
      }

      if (poll_status == PGRES_POLLING_OK) {
        // Connection successful
        break;
      }

      // Need to poll
      if (poll_status == PGRES_POLLING_READING) {
        // Wait until socket is readable
        boost::system::error_code ec;
        co_await socket().async_wait(boost::asio::ip::tcp::socket::wait_read,
                                     boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec) {
          close();
          co_return std::unexpected(PgError{.message = ec.message(), .error_code = ec.value()});
        }
      } else if (poll_status == PGRES_POLLING_WRITING) {
        // Wait until socket is writable
        boost::system::error_code ec;
        co_await socket().async_wait(boost::asio::ip::tcp::socket::wait_write,
                                     boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec) {
          close();
          co_return std::unexpected(PgError{.message = ec.message(), .error_code = ec.value()});
        }
      }
    }

    if (PQstatus(conn_) != CONNECTION_OK) {
      auto error = PgError::from_conn(conn_);
      close();
      co_return std::unexpected(error);
    }

    co_return PgResult<void>{};
  }

  // Asynchronous parameterized query execution using boost::asio::awaitable
  boost::asio::awaitable<PgResult<Result>> query(const std::string& query_text,
                                                 const std::vector<std::string>& params = {}) {
    if (!is_open()) {
      co_return std::unexpected(PgError{.message = "Connection is not open", .error_code = -1});
    }

    // Convert parameters
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& param : params) {
      values.push_back(param.c_str());
    }

    // Send the parameterized query
    if (!PQsendQueryParams(conn_, query_text.c_str(), static_cast<int>(values.size()),
                           // TODO allow user to customize fields with nullptr values
                           nullptr,  // param types - inferred
                           values.size() == 0 ? nullptr : values.data(),
                           nullptr,  // param lengths - null-terminated strings
                           nullptr,  // param formats - text format
                           0         // result format - text format
                           )) {
      co_return std::unexpected(PgError::from_conn(conn_));
    }

    // Flush the outgoing data
    auto flush_result = co_await flush_outgoing_data();
    if (!flush_result) {
      co_return std::unexpected(flush_result.error());
    }

    // Get and return the result
    co_return co_await get_query_result();
  }

  // Transaction support
  // ------------------

  // Begin transaction with specified isolation level
  boost::asio::awaitable<PgResult<void>> begin_transaction(
      IsolationLevel isolation = IsolationLevel::ReadCommitted) {
    if (in_transaction_) {
      co_return std::unexpected(PgError{.message = "Already in a transaction", .error_code = -1});
    }

    std::string isolation_str;
    switch (isolation) {
    case IsolationLevel::ReadUncommitted:
      isolation_str = "READ UNCOMMITTED";
      break;
    case IsolationLevel::ReadCommitted:
      isolation_str = "READ COMMITTED";
      break;
    case IsolationLevel::RepeatableRead:
      isolation_str = "REPEATABLE READ";
      break;
    case IsolationLevel::Serializable:
      isolation_str = "SERIALIZABLE";
      break;
    }

    const std::string begin_cmd = "BEGIN ISOLATION LEVEL " + isolation_str;
    auto res_result = co_await query(begin_cmd);

    if (!res_result) {
      co_return std::unexpected(res_result.error());
    }

    if (!*res_result) {
      co_return std::unexpected(PgError{.message = res_result->error_message(),
                                        .error_code = static_cast<int>(res_result->status())});
    }

    in_transaction_ = true;
    co_return PgResult<void>{};
  }

  // Commit the current transaction
  boost::asio::awaitable<PgResult<void>> commit() {
    if (!in_transaction_) {
      co_return std::unexpected(PgError{.message = "Not in a transaction", .error_code = -1});
    }

    auto res_result = co_await query("COMMIT");

    if (!res_result) {
      co_return std::unexpected(res_result.error());
    }

    if (!*res_result) {
      co_return std::unexpected(PgError{.message = res_result->error_message(),
                                        .error_code = static_cast<int>(res_result->status())});
    }

    in_transaction_ = false;
    co_return PgResult<void>{};
  }

  // Rollback the current transaction
  boost::asio::awaitable<PgResult<void>> rollback() {
    if (!in_transaction_) {
      co_return std::unexpected(PgError{.message = "Not in a transaction", .error_code = -1});
    }

    auto res_result = co_await query("ROLLBACK");

    if (!res_result) {
      co_return std::unexpected(res_result.error());
    }

    if (!*res_result) {
      co_return std::unexpected(PgError{.message = res_result->error_message(),
                                        .error_code = static_cast<int>(res_result->status())});
    }

    in_transaction_ = false;
    co_return PgResult<void>{};
  }

  // Prepared statement support
  // -------------------------

  // Create a prepared statement
  boost::asio::awaitable<PgResult<std::shared_ptr<PreparedStatement>>> prepare_statement(
      const std::string& name, const std::string& query_text) {
    if (!is_open()) {
      co_return std::unexpected(PgError{.message = "Connection is not open", .error_code = -1});
    }

    auto it = statements_.find(name);
    if (it != statements_.end()) {
      // Statement with this name already exists
      if (it->second->query() != query_text) {
        // Deallocate the old statement since the query is different
        auto deallocate_result = co_await it->second->deallocate();
        if (!deallocate_result) {
          co_return std::unexpected(deallocate_result.error());
        }
        statements_.erase(it);
      } else {
        // Return the existing statement if it has the same query
        co_return it->second;
      }
    }

    // Create a new prepared statement
    auto stmt = std::make_shared<PreparedStatement>(*this, name, query_text);
    statements_[name] = stmt;

    // Prepare it
    auto prepare_result = co_await stmt->prepare();
    if (!prepare_result) {
      statements_.erase(name);
      co_return std::unexpected(prepare_result.error());
    }

    co_return stmt;
  }

  // Get a prepared statement by name
  PgResult<std::shared_ptr<PreparedStatement>> get_prepared_statement(const std::string& name) {
    auto it = statements_.find(name);
    if (it != statements_.end()) {
      return it->second;
    }
    return std::unexpected(
        PgError{.message = "Prepared statement not found: " + name, .error_code = -1});
  }

  // Execute a prepared statement by name
  boost::asio::awaitable<PgResult<Result>> execute_prepared(
      const std::string& name, const std::vector<std::string>& params = {}) {
    auto stmt_result = get_prepared_statement(name);
    if (!stmt_result) {
      co_return std::unexpected(stmt_result.error());
    }

    co_return co_await (*stmt_result)->execute(params);
  }

  // Deallocate a prepared statement by name
  boost::asio::awaitable<PgResult<void>> deallocate_prepared(const std::string& name) {
    auto stmt_result = get_prepared_statement(name);
    if (!stmt_result) {
      co_return std::unexpected(stmt_result.error());
    }

    auto deallocate_result = co_await (*stmt_result)->deallocate();
    if (!deallocate_result) {
      co_return std::unexpected(deallocate_result.error());
    }

    statements_.erase(name);
    co_return PgResult<void>{};
  }

  // Deallocate all prepared statements
  boost::asio::awaitable<PgResult<void>> deallocate_all_prepared() {
    std::vector<std::string> names;
    names.reserve(statements_.size());

    for (auto& [name, _] : statements_) {
      names.push_back(name);
    }

    for (const auto& name : names) {
      auto deallocate_result = co_await deallocate_prepared(name);
      if (!deallocate_result) {
        co_return std::unexpected(deallocate_result.error());
      }
    }

    co_return PgResult<void>{};
  }

  friend class PreparedStatement;
};

}  // namespace relx::pgsql_async_wrapper
