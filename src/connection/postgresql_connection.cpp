#include "relx/connection/postgresql_connection.hpp"

#include "relx/connection/meta.hpp"
#include "relx/connection/postgresql_statement.hpp"

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

// Helper function to convert PostgreSQL hex BYTEA format to binary
std::string convert_pg_bytea_to_binary(const std::string& hex_value) {
  // Check if this is a PostgreSQL hex-encoded BYTEA value (starts with \x)
  if (hex_value.size() >= 2 && hex_value.substr(0, 2) == "\\x") {
    std::string binary_result;
    binary_result.reserve((hex_value.size() - 2) / 2);

    // Skip the \x prefix and process each hex byte
    for (size_t i = 2; i < hex_value.size(); i += 2) {
      if (i + 1 < hex_value.size()) {
        try {
          std::string hex_byte = hex_value.substr(i, 2);
          char byte = static_cast<char>(std::stoi(hex_byte, nullptr, 16));
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
    std::string error_msg = PQerrorMessage(pg_conn_);
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

  ExecStatusType status = PQresultStatus(result);

  if (expected_status != -1 && status != expected_status) {
    std::string error_msg = PQresultErrorMessage(result);
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
    std::string pg_sql = convert_placeholders(sql);

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
  ExecStatusType status = PQresultStatus(pg_result.get());

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
    std::string error_msg = PQresultErrorMessage(pg_result.get());
    return std::unexpected(ConnectionError{.message = "PostgreSQL error: " + error_msg,
                                           .error_code = static_cast<int>(status)});
  }

  // Create result set
  std::vector<std::string> column_names;
  std::vector<result::Row> rows;

  // Get column names
  int column_count = PQnfields(pg_result.get());
  column_names.reserve(column_count);
  for (int i = 0; i < column_count; i++) {
    const char* name = PQfname(pg_result.get(), i);
    column_names.push_back(name ? name : "");
  }

  // Process rows
  int row_count = PQntuples(pg_result.get());
  rows.reserve(row_count);

  for (int row_idx = 0; row_idx < row_count; row_idx++) {
    std::vector<result::Cell> cells;
    cells.reserve(column_count);

    for (int col_idx = 0; col_idx < column_count; col_idx++) {
      if (PQgetisnull(pg_result.get(), row_idx, col_idx)) {
        cells.emplace_back("NULL");
      } else {
        const char* value = PQgetvalue(pg_result.get(), row_idx, col_idx);
        cells.emplace_back(value ? value : "");
      }
    }

    rows.push_back(result::Row(std::move(cells), column_names));
  }

  // Create the result set from the data we collected
  return result::ResultSet(std::move(rows), std::move(column_names));
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
    std::string pg_sql = convert_placeholders(sql);

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

  // Process the result set - same as in execute_raw
  std::vector<std::string> column_names;
  std::vector<result::Row> rows;

  // Get column names
  int column_count = PQnfields(pg_result.get());
  column_names.reserve(column_count);
  for (int i = 0; i < column_count; i++) {
    const char* name = PQfname(pg_result.get(), i);
    column_names.push_back(name ? name : "");
  }

  // Process rows
  int row_count = PQntuples(pg_result.get());
  rows.reserve(row_count);

  // Get column types to identify BYTEA columns
  std::vector<bool> is_bytea_column(column_count, false);
  for (int i = 0; i < column_count; i++) {
    // PostgreSQL BYTEA type OID is 17
    is_bytea_column[i] = (PQftype(pg_result.get(), i) == 17);
  }

  for (int row_idx = 0; row_idx < row_count; row_idx++) {
    std::vector<result::Cell> cells;
    cells.reserve(column_count);

    for (int col_idx = 0; col_idx < column_count; col_idx++) {
      if (PQgetisnull(pg_result.get(), row_idx, col_idx)) {
        cells.emplace_back("NULL");
      } else {
        const char* value = PQgetvalue(pg_result.get(), row_idx, col_idx);
        std::string cell_value = value ? value : "";

        // Automatically convert BYTEA data from hex to binary
        if (is_bytea_column[col_idx]) {
          cell_value = convert_pg_bytea_to_binary(cell_value);
        }

        cells.emplace_back(std::move(cell_value));
      }
    }

    rows.push_back(result::Row(std::move(cells), column_names));
  }

  // Create the result set from the data we collected
  return result::ResultSet(std::move(rows), std::move(column_names));
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
  std::string isolation_level_str;
  switch (isolation_level) {
  case IsolationLevel::ReadUncommitted:
    isolation_level_str = "READ UNCOMMITTED";
    break;
  case IsolationLevel::ReadCommitted:
    isolation_level_str = "READ COMMITTED";
    break;
  case IsolationLevel::RepeatableRead:
    isolation_level_str = "REPEATABLE READ";
    break;
  case IsolationLevel::Serializable:
    isolation_level_str = "SERIALIZABLE";
    break;
  default:
    isolation_level_str = "READ COMMITTED";
    break;
  }

  // Execute the transaction begin statement with isolation level
  std::string begin_sql = "BEGIN ISOLATION LEVEL " + isolation_level_str;
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
  std::regex placeholder_regex("\\?");
  std::string result;
  std::string::const_iterator search_start(sql.cbegin());
  std::smatch match;
  int placeholder_count = 1;

  while (std::regex_search(search_start, sql.cend(), match, placeholder_regex)) {
    result.append(search_start, match[0].first);
    result.append("$" + std::to_string(placeholder_count++));
    search_start = match[0].second;
  }

  result.append(search_start, sql.cend());
  return result;
}

std::unique_ptr<PostgreSQLStatement> PostgreSQLConnection::prepare_statement(
    const std::string& name, const std::string& sql, int param_count) {
  if (!is_connected_ || !pg_conn_) {
    throw ConnectionError{.message = "Not connected to database", .error_code = -1};
  }

  // Convert ? placeholders to $1, $2, etc.
  std::string pg_sql = convert_placeholders(sql);

  // Prepare the statement in PostgreSQL
  PGResultWrapper result(PQprepare(pg_conn_, name.c_str(), pg_sql.c_str(), param_count,
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