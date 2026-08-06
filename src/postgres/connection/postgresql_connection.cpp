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
    // Capture the message and status before PQfinish - afterwards the handle is
    // gone and PQstatus(nullptr) would flatten every failure to CONNECTION_BAD
    const std::string error_msg = PQerrorMessage(pg_conn_);
    const ConnStatusType status = PQstatus(pg_conn_);
    PQfinish(pg_conn_);
    pg_conn_ = nullptr;
    return std::unexpected(
        ConnectionError{.message = "Failed to connect to PostgreSQL database: " + error_msg,
                        .error_code = static_cast<int>(status)});
  }

  is_connected_ = true;
  return {};
}

ConnectionResult<void> PostgreSQLConnection::disconnect() {
  if (!is_connected_ || !pg_conn_) {
    is_connected_ = false;
    in_transaction_ = false;
    pg_conn_ = nullptr;
    cached_statements_.clear();
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
  cached_statements_.clear();  // server-side prepared statements died with the session
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

namespace {

/// @brief Map a PGresult's status to success or a ConnectionError
ConnectionResult<void> validate_exec_status(PGresult* result) {
  const ExecStatusType status = PQresultStatus(result);

  switch (status) {
  case PGRES_COMMAND_OK:
  case PGRES_TUPLES_OK:
  case PGRES_SINGLE_TUPLE:
    // These are all success cases
    return {};

  case PGRES_EMPTY_QUERY:
    return std::unexpected(ConnectionError{.message = "Empty query string was executed",
                                           .error_code = static_cast<int>(status)});

  case PGRES_NONFATAL_ERROR:
    // Server-side notice; libpq delivers these through PQsetNoticeReceiver, and a
    // library must not write to stderr on its own
    return {};

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
  default:
    // Attach the server diagnostics so callers can classify the failure
    // (e.g. ConnectionError::is_duplicate_key_error) instead of parsing the message
    auto diag = [result](int field) {
      const char* value = PQresultErrorField(result, field);
      return value ? std::string(value) : std::string();
    };
    return std::unexpected(ConnectionError{
        .message = "PostgreSQL error: " + std::string(PQresultErrorMessage(result)),
        .error_code = static_cast<int>(status),
        .sql_state = diag(PG_DIAG_SQLSTATE),
        .detail = diag(PG_DIAG_MESSAGE_DETAIL),
        .hint = diag(PG_DIAG_MESSAGE_HINT),
        .constraint_name = diag(PG_DIAG_CONSTRAINT_NAME),
    });
  }
}

}  // namespace

// Nice for debugging
constexpr bool ultra_verbose = false;
ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_raw(
    const std::string& sql, const std::vector<bind_param>& params) {
  return execute_params_internal(sql, params, /*binary_results=*/false);
}

ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_raw_binary_result(
    const std::string& sql, const std::vector<bind_param>& params) {
  return execute_params_internal(sql, params, /*binary_results=*/true);
}

ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_params_internal(
    const std::string& sql, const std::vector<bind_param>& params, bool binary_results) {
  if constexpr (ultra_verbose) {
    std::cout << "Executing raw SQL: " << sql << std::endl;
    for (const auto& param : params) {
      std::cout << "Param: " << param.value << std::endl;
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

  if (params.empty() && !binary_results) {
    // Execute without parameters (PQexec also supports multi-statement SQL)
    pg_result = PGResultWrapper(PQexec(pg_conn_, sql.c_str()));
  } else if (params.empty()) {
    // Binary results need PQexecParams; typed queries are single statements
    pg_result = PGResultWrapper(PQexecParams(pg_conn_, sql.c_str(), 0, nullptr, nullptr, nullptr,
                                             nullptr, 1 /* binary results */));
  } else {
    // Convert ? placeholders to $1, $2, etc.
    const std::string pg_sql = convert_placeholders(sql);

    // Typed params (bool/int/float from the query builder) go over the binary protocol
    // with their type OID; untyped ones as text with the type left to server inference
    std::vector<Oid> param_types;
    std::vector<const char*> param_values;
    std::vector<int> param_lengths;
    std::vector<int> param_formats;
    param_types.reserve(params.size());
    param_values.reserve(params.size());
    param_lengths.reserve(params.size());
    param_formats.reserve(params.size());

    for (const auto& param : params) {
      param_types.push_back(static_cast<Oid>(param.kind));
      if (param.is_null) {
        param_values.push_back(nullptr);  // SQL NULL
        param_lengths.push_back(0);
        param_formats.push_back(0);
      } else if (param.kind != sql_kind::unspecified) {
        param_values.push_back(reinterpret_cast<const char*>(param.binary.data()));
        param_lengths.push_back(static_cast<int>(param.binary.size()));
        param_formats.push_back(1);  // binary
      } else {
        param_values.push_back(param.value.c_str());
        param_lengths.push_back(static_cast<int>(param.value.size()));
        param_formats.push_back(0);  // text
      }
    }

    // Execute with parameters
    pg_result = PGResultWrapper(PQexecParams(
        pg_conn_, pg_sql.c_str(), static_cast<int>(params.size()), param_types.data(),
        param_values.data(), param_lengths.data(), param_formats.data(), binary_results ? 1 : 0));
  }

  // Check if memory allocation failed
  if (!pg_result.get()) {
    return std::unexpected(ConnectionError{.message = "Failed to execute query", .error_code = -1});
  }

  if (auto status_ok = validate_exec_status(pg_result.get()); !status_ok) {
    return std::unexpected(status_ok.error());
  }

  // Process result using shared utility function
  if (binary_results) {
    auto decoded = sql_utils::process_postgresql_result_binary(pg_result.get());
    if (!decoded) {
      return std::unexpected(ConnectionError{
          .message = "Failed to decode binary result: " + decoded.error(), .error_code = -1});
    }
    return *decoded;
  }
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
  try {
    return sql_utils::process_postgresql_result(pg_result.get(), true);
  } catch (const std::exception& e) {
    return std::unexpected(ConnectionError{
        .message = std::string("Failed to decode BYTEA result: ") + e.what(), .error_code = -1});
  }
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

ConnectionResult<std::unique_ptr<PostgreSQLStatement>> PostgreSQLConnection::prepare_statement(
    const std::string& name, const std::string& sql, int param_count) {
  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  // Convert ? placeholders to $1, $2, etc.
  const std::string pg_sql = convert_placeholders(sql);

  // Prepare the statement in PostgreSQL
  const PGResultWrapper result(PQprepare(pg_conn_, name.c_str(), pg_sql.c_str(), param_count,
                                         nullptr  // Use default parameter types
                                         ));

  auto result_handler = handle_pg_result(result.get(), PGRES_COMMAND_OK);
  if (!result_handler) {
    return std::unexpected(result_handler.error());
  }

  // Create and return the statement object
  return std::make_unique<PostgreSQLStatement>(*this, name, sql, param_count);
}

ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_prepared(
    const std::string& statement_name, const std::vector<std::optional<std::string>>& params) {
  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  std::vector<const char*> param_values;
  param_values.reserve(params.size());
  for (const auto& param : params) {
    param_values.push_back(param ? param->c_str() : nullptr);  // nullptr binds SQL NULL
  }

  const PGResultWrapper pg_result(PQexecPrepared(pg_conn_, statement_name.c_str(),
                                                 static_cast<int>(params.size()),
                                                 param_values.data(), nullptr, nullptr,
                                                 0  // text-format results
                                                 ));

  if (!pg_result.get()) {
    return std::unexpected(
        ConnectionError{.message = "Failed to execute prepared statement", .error_code = -1});
  }

  if (auto status_ok = validate_exec_status(pg_result.get()); !status_ok) {
    return std::unexpected(status_ok.error());
  }

  return sql_utils::process_postgresql_result(pg_result.get(), false);
}

ConnectionResult<result::ResultSet> PostgreSQLConnection::execute_static_statement(
    const std::string& sql, const std::vector<bind_param>& params, bool binary_results,
    const std::type_info& query_key) {
  if (!statement_cache_enabled_) {
    return execute_params_internal(sql, params, binary_results);
  }

  if (!is_connected_ || !pg_conn_) {
    return std::unexpected(
        ConnectionError{.message = "Not connected to database", .error_code = -1});
  }

  const std::type_index key(query_key);
  auto it = cached_statements_.find(key);
  if (it == cached_statements_.end()) {
    // First execution of this query type on this connection: prepare it, with the
    // parameter type OIDs the query binds (kinds are derived from the bound value
    // types, so they are identical for every execution of the same query type)
    std::string name = "relx_ps_" + std::to_string(cached_statements_.size());
    const std::string pg_sql = convert_placeholders(sql);

    std::vector<Oid> param_types;
    param_types.reserve(params.size());
    for (const auto& param : params) {
      param_types.push_back(static_cast<Oid>(param.kind));
    }

    const PGResultWrapper prepared(PQprepare(pg_conn_, name.c_str(), pg_sql.c_str(),
                                             static_cast<int>(params.size()),
                                             param_types.empty() ? nullptr : param_types.data()));
    if (auto status_ok = handle_pg_result(prepared.get(), PGRES_COMMAND_OK); !status_ok) {
      return std::unexpected(status_ok.error());
    }
    it = cached_statements_.emplace(key, std::move(name)).first;
  }

  // Same parameter marshalling as execute_params_internal: typed params travel as
  // binary with their prepared OIDs, untyped ones as text
  std::vector<const char*> param_values;
  std::vector<int> param_lengths;
  std::vector<int> param_formats;
  param_values.reserve(params.size());
  param_lengths.reserve(params.size());
  param_formats.reserve(params.size());

  for (const auto& param : params) {
    if (param.is_null) {
      param_values.push_back(nullptr);  // SQL NULL
      param_lengths.push_back(0);
      param_formats.push_back(0);
    } else if (param.kind != sql_kind::unspecified) {
      param_values.push_back(reinterpret_cast<const char*>(param.binary.data()));
      param_lengths.push_back(static_cast<int>(param.binary.size()));
      param_formats.push_back(1);  // binary
    } else {
      param_values.push_back(param.value.c_str());
      param_lengths.push_back(static_cast<int>(param.value.size()));
      param_formats.push_back(0);  // text
    }
  }

  const PGResultWrapper pg_result(PQexecPrepared(
      pg_conn_, it->second.c_str(), static_cast<int>(params.size()), param_values.data(),
      param_lengths.data(), param_formats.data(), binary_results ? 1 : 0));

  if (!pg_result.get()) {
    return std::unexpected(
        ConnectionError{.message = "Failed to execute prepared statement", .error_code = -1});
  }

  if (auto status_ok = validate_exec_status(pg_result.get()); !status_ok) {
    return std::unexpected(status_ok.error());
  }

  if (binary_results) {
    auto decoded = sql_utils::process_postgresql_result_binary(pg_result.get());
    if (!decoded) {
      return std::unexpected(ConnectionError{
          .message = "Failed to decode binary result: " + decoded.error(), .error_code = -1});
    }
    return *decoded;
  }
  return sql_utils::process_postgresql_result(pg_result.get(), false);
}

}  // namespace relx::connection