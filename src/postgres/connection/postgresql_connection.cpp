#include "relx/connection/postgresql_connection.hpp"

#include "relx/connection/meta.hpp"
#include "relx/connection/postgresql_statement.hpp"
#include "relx/connection/sql_utils.hpp"

#include <iostream>
#include <regex>
#include <stdexcept>
#include <vector>

#include <libpq-fe.h>
namespace relx::connection {

// RAII wrapper for PGresult
class PGResultWrapper {
public:
  explicit PGResultWrapper(PGresult* result) : result_(result) {}
  ~PGResultWrapper() {
    if (result_) {
      PQclear(result_);
    }
  }

  // Delete copy constructor and assignment
  PGResultWrapper(const PGResultWrapper&) = delete;
  PGResultWrapper& operator=(const PGResultWrapper&) = delete;

  // Allow move
  PGResultWrapper(PGResultWrapper&& other) noexcept : result_(other.result_) {
    other.result_ = nullptr;
  }
  PGResultWrapper& operator=(PGResultWrapper&& other) noexcept {
    if (this != &other) {
      if (result_) {
        PQclear(result_);
      }
      result_ = other.result_;
      other.result_ = nullptr;
    }
    return *this;
  }

  // Get the raw pointer
  PGresult* get() const { return result_; }
  PGresult* operator->() const { return result_; }
  PGresult* release() {
    PGresult* result = result_;
    result_ = nullptr;
    return result;
  }

  // reset with new result
  void reset(PGresult* result) {
    if (result_) {
      PQclear(result_);
    }
    result_ = result;
  }

private:
  PGresult* result_;
};

PostgreSQLConnection::PostgreSQLConnection(std::string_view connection_string)
    : connection_string_(connection_string) {}

PostgreSQLConnection::PostgreSQLConnection(const PostgreSQLConnectionParams& params)
    : connection_string_(params.to_connection_string()) {}

PostgreSQLConnection::~PostgreSQLConnection() {
  disconnect();
}

PostgreSQLConnection::PostgreSQLConnection(PostgreSQLConnection&& other) noexcept
    : connection_string_(std::move(other.connection_string_)), pg_conn_(other.pg_conn_),
      is_connected_(other.is_connected_), in_transaction_(other.in_transaction_) {
  other.pg_conn_ = nullptr;
  other.is_connected_ = false;
  other.in_transaction_ = false;
}

PostgreSQLConnection& PostgreSQLConnection::operator=(PostgreSQLConnection&& other) noexcept {
  if (this != &other) {
    disconnect();
    connection_string_ = std::move(other.connection_string_);
    pg_conn_ = other.pg_conn_;
    is_connected_ = other.is_connected_;
    in_transaction_ = other.in_transaction_;
    other.pg_conn_ = nullptr;
    other.is_connected_ = false;
    other.in_transaction_ = false;
  }
  return *this;
}

ConnectionResult<void> PostgreSQLConnection::connect() {
  if (is_connected_) {
    return {};  // Already connected
  }

  pg_conn_ = PQconnectdb(connection_string_.c_str());

  if (PQstatus(pg_conn_) != CONNECTION_OK) {
    const std::string error_msg = PQerrorMessage(pg_conn_);
    PQfinish(pg_conn_);
    pg_conn_ = nullptr;
    return std::unexpected(
        ConnectionError{.message = "Failed to connect to PostgreSQL database: " + error_msg,
                        .error_code = static_cast<int>(PQstatus(pg_conn_))});
  }

  is_connected_ = true;
  return {};
}

ConnectionResult<void> PostgreSQLConnection::disconnect() {
  if (!is_connected_ || !pg_conn_) {
    is_connected_ = false;
    in_transaction_ = false;
    pg_conn_ = nullptr;
    return {};  // Already disconnected
  }

  // If there's an active transaction, roll it back before disconnecting
  if (in_transaction_) {
    auto rollback_result = rollback_transaction();
    if (!rollback_result) {
      // Just log the error and continue with disconnect
      // We don't return here because we still want to try to close the connection
    }
  }

  PQfinish(pg_conn_);
  is_connected_ = false;
  in_transaction_ = false;
  pg_conn_ = nullptr;
  return {};
}

ConnectionResult<PGresult*> PostgreSQLConnection::handle_pg_result(PGresult* result,
                                                                   int expected_status) {
  if (!result) {
    return std::unexpected(ConnectionError{.message = PQerrorMessage(pg_conn_),
                                           .error_code = static_cast<int>(PQstatus(pg_conn_))});
  }

  const ExecStatusType status = PQresultStatus(result);

  if (expected_status != -1 && status != expected_status) {
    const std::string error_msg = PQresultErrorMessage(result);
    return std::unexpected(ConnectionError{.message = "PostgreSQL error: " + error_msg,
                                           .error_code = static_cast<int>(status)});
  }

  return result;
}

// Nice for debugging
constexpr bool ultra_verbose = false;
ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_raw(
    const std::string& sql, const std::vector<std::string>& params) {
  if constexpr (ultra_verbose) {
    std::println("Executing raw SQL: {}", sql);
    for (const auto& param : params) {
      std::println("Param: {}", param);
    }
  }

  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  // Check connection status
  if (PQstatus(pg_conn_) != CONNECTION_OK) {
    return std::unexpected(ConnectionError{.message = "Connection is not in OK state: " +
                                                      std::string(PQerrorMessage(pg_conn_)),
                                           .error_code = static_cast<int>(PQstatus(pg_conn_))});
  }

  PGResultWrapper pg_result(nullptr);

  if (params.empty()) {
    // Execute without parameters
    pg_result = PGResultWrapper(PQexec(pg_conn_, sql.c_str()));
  } else {
    // Convert ? placeholders to $1, $2, etc.
    const std::string pg_sql = convert_placeholders(sql);

    // Prepare parameter values array
    std::vector<const char*> param_values;
    param_values.reserve(params.size());

    for (const auto& param : params) {
      param_values.push_back(param.c_str());
    }

    // Execute with parameters
    pg_result = PGResultWrapper(PQexecParams(pg_conn_, pg_sql.c_str(),
                                             static_cast<int>(params.size()),
                                             nullptr,  // Use default parameter types
                                             param_values.data(),
                                             nullptr,  // All parameters are text format
                                             nullptr,  // All parameters are text format
                                             0         // Use text format for results
                                             ));
  }

  // Check if memory allocation failed
  if (!pg_result.get()) {
    return std::unexpected(ConnectionError{.message = "Failed to execute query", .error_code = -1});
  }

  // Handle the result
  const ExecStatusType status = PQresultStatus(pg_result.get());

  // Handle different result statuses
  switch (status) {
  case PGRES_COMMAND_OK:
  case PGRES_TUPLES_OK:
  case PGRES_SINGLE_TUPLE:
    // These are all success cases
    break;

  case PGRES_EMPTY_QUERY:
    return std::unexpected(ConnectionError{.message = "Empty query string was executed",
                                           .error_code = static_cast<int>(status)});

  case PGRES_NONFATAL_ERROR:
    // Log the warning but continue processing
    std::print("PostgreSQL warning: {}", PQresultErrorMessage(pg_result.get()));
    break;

  case PGRES_COPY_IN:
  case PGRES_COPY_OUT:
  case PGRES_COPY_BOTH:
    return std::unexpected(
        ConnectionError{.message = "COPY operations are not supported in this context",
                        .error_code = static_cast<int>(status)});

  case PGRES_PIPELINE_SYNC:
    return std::unexpected(
        ConnectionError{.message = "Pipeline operations are not supported in this context",
                        .error_code = static_cast<int>(status)});

  case PGRES_BAD_RESPONSE:
  case PGRES_FATAL_ERROR:
  case PGRES_PIPELINE_ABORTED:
    const std::string error_msg = PQresultErrorMessage(pg_result.get());
    return std::unexpected(ConnectionError{.message = "PostgreSQL error: " + error_msg,
                                           .error_code = static_cast<int>(status)});
  }

  // Process result using shared utility function
  return sql_utils::process_postgresql_result(pg_result.get(), false);
}

ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_raw_binary(
    const std::string& sql, const std::vector<std::string>& params,
    const std::vector<bool>& is_binary) {
  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  if (params.size() != is_binary.size()) {
    return std::unexpected(
        ConnectionError{.message = "Parameter count mismatch with binary flags", .error_code = -1});
  }

  PGResultWrapper pg_result(nullptr);

  if (params.empty()) {
    // Execute without parameters
    pg_result = PGResultWrapper(PQexec(pg_conn_, sql.c_str()));
  } else {
    // Convert ? placeholders to $1, $2, etc.
    const std::string pg_sql = convert_placeholders(sql);

    // Prepare parameter values and format arrays
    std::vector<const char*> param_values;
    std::vector<int> param_formats;
    std::vector<int> param_lengths;

    param_values.reserve(params.size());
    param_formats.reserve(params.size());
    param_lengths.reserve(params.size());

    for (size_t i = 0; i < params.size(); ++i) {
      param_values.push_back(params[i].c_str());
      param_lengths.push_back(static_cast<int>(params[i].size()));
      param_formats.push_back(is_binary[i] ? 1 : 0);  // 1 for binary, 0 for text
    }

    // Execute with parameters
    pg_result = PGResultWrapper(
        PQexecParams(pg_conn_, pg_sql.c_str(), static_cast<int>(params.size()),
                     nullptr,  // Use default parameter types
                     param_values.data(), param_lengths.data(), param_formats.data(),
                     0  // Use text format for results
                     ));
  }

  auto result_handler = handle_pg_result(pg_result.get());
  if (!result_handler) {
    return std::unexpected(result_handler.error());
  }

  // Process result using shared utility function with BYTEA conversion
  return sql_utils::process_postgresql_result(pg_result.get(), true);
}

bool PostgreSQLConnection::is_connected() const {
  return is_connected_ && pg_conn_ != nullptr && PQstatus(pg_conn_) == CONNECTION_OK;
}

ConnectionResult<void> PostgreSQLConnection::begin_transaction(IsolationLevel isolation_level) {
  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  if (in_transaction_) {
    return std::unexpected(
        ConnectionError{.message = "Transaction already in progress", .error_code = -1});
  }

  // Map isolation level to PostgreSQL transaction type
  const std::string isolation_level_str = sql_utils::isolation_level_to_postgresql_string(
      static_cast<int>(isolation_level));

  // Execute the transaction begin statement with isolation level
  const std::string begin_sql = "BEGIN ISOLATION LEVEL " + isolation_level_str;
  auto result = execute_raw(begin_sql);
  if (!result) {
    return std::unexpected(result.error());
  }

  in_transaction_ = true;
  return {};
}

ConnectionResult<void> PostgreSQLConnection::commit_transaction() {
  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  if (!in_transaction_) {
    return std::unexpected(
        ConnectionError{.message = "No transaction in progress", .error_code = -1});
  }

  // Execute the commit statement
  auto result = execute_raw("COMMIT");
  if (!result) {
    return std::unexpected(result.error());
  }

  in_transaction_ = false;
  return {};
}

ConnectionResult<void> PostgreSQLConnection::rollback_transaction() {
  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  if (!in_transaction_) {
    return std::unexpected(
        ConnectionError{.message = "No transaction in progress", .error_code = -1});
  }

  // Execute the rollback statement
  auto result = execute_raw("ROLLBACK");
  if (!result) {
    return std::unexpected(result.error());
  }

  in_transaction_ = false;
  return {};
}

bool PostgreSQLConnection::in_transaction() const {
  return in_transaction_;
}

std::string PostgreSQLConnection::convert_placeholders(const std::string& sql) {
  return sql_utils::convert_placeholders_to_postgresql(sql);
}

std::unique_ptr<PostgreSQLStatement> PostgreSQLConnection::prepare_statement(
    const std::string& name, const std::string& sql, int param_count) {
  if (!is_connected_ || !pg_conn_) {
    throw ConnectionError{.message = "Not connected to database", .error_code = -1};
  }

  // Convert ? placeholders to $1, $2, etc.
  const std::string pg_sql = convert_placeholders(sql);

  // Prepare the statement in PostgreSQL
  const PGResultWrapper result(PQprepare(pg_conn_, name.c_str(), pg_sql.c_str(), param_count,
                                         nullptr  // Use default parameter types
                                         ));

  auto result_handler = handle_pg_result(result.get());
  if (!result_handler) {
    throw result_handler.error();
  }

  // Create and return the statement object
  return std::make_unique<PostgreSQLStatement>(*this, name, sql, param_count);
}

}  // namespace relx::connection