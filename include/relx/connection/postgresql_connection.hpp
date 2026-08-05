#pragma once

#include "connection.hpp"

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Forward declarations to avoid including libpq headers in our public API
struct pg_conn;
using PGconn = pg_conn;
struct pg_result;
using PGresult = pg_result;

// Forward declare statement class
namespace relx::connection {
class PostgreSQLStatement;
}  // namespace relx::connection

namespace relx::connection {

/// @brief PostgreSQL implementation of the Connection interface
class PostgreSQLConnection : public Connection {
public:
  /// @brief Constructor with connection parameters
  /// @param connection_string PostgreSQL connection string (e.g. "host=localhost port=5432
  /// dbname=mydb user=postgres password=password")
  explicit PostgreSQLConnection(std::string_view connection_string);

  /// @brief Constructor with structured connection parameters
  /// @param params PostgreSQL connection parameters
  explicit PostgreSQLConnection(const PostgreSQLConnectionParams& params);

  /// @brief Destructor that ensures proper cleanup
  ~PostgreSQLConnection() override;

  // Delete copy constructor and assignment operator
  PostgreSQLConnection(const PostgreSQLConnection&) = delete;
  PostgreSQLConnection& operator=(const PostgreSQLConnection&) = delete;

  // Allow move operations
  PostgreSQLConnection(PostgreSQLConnection&&) noexcept;
  PostgreSQLConnection& operator=(PostgreSQLConnection&&) noexcept;

  /// @brief Connect to the PostgreSQL database
  /// @return Result indicating success or failure
  ConnectionResult<void> connect() override;

  /// @brief Disconnect from the PostgreSQL database
  /// @return Result indicating success or failure
  ConnectionResult<void> disconnect() override;

  /// @brief Execute a raw SQL query with parameters
  /// @param sql The SQL query string
  /// @param params Parameter values; params tagged with a sql_kind (bool/int/float
  /// values bound through the query builder) are sent with their type OID in binary
  /// format, untagged params as untyped text
  /// @return Result containing the query results or an error
  ConnectionResult<result::ResultSet> execute_raw(
      const std::string& sql, const std::vector<bind_param>& params = {}) override;

  /// @brief Execute a query requesting binary-format results, decoded back into
  /// canonical text cells (see Connection::execute_raw_binary_result)
  ConnectionResult<result::ResultSet> execute_raw_binary_result(
      const std::string& sql, const std::vector<bind_param>& params = {}) override;

  /// @brief Execute a raw SQL query with binary parameters
  /// @param sql The SQL query string
  /// @param params Vector of parameter values
  /// @param is_binary Vector of flags indicating whether each parameter is binary
  /// @return Result containing the query results or an error
  // TODO make binary type for bytea type in postgresql instead of this hacky function
  ConnectionResult<result::ResultSet> execute_raw_binary(
      const std::string& sql, const std::vector<std::string>& params,
      const std::vector<bool>& is_binary);  // TODO replace vector bool

  /// @brief Execute a raw SQL query with typed parameters
  /// @tparam Args The types of the parameters
  /// @param sql The SQL query string
  /// @param args The parameter values
  /// @return Result containing the query results or an error
  template <typename... Args>
  ConnectionResult<result::ResultSet> execute_typed(const std::string& sql, Args&&... args) {
    std::vector<bind_param> params;
    params.reserve(sizeof...(Args));

    auto add_param = [&params](auto&& param) {
      using ParamType = std::remove_cvref_t<decltype(param)>;

      if constexpr (std::is_same_v<ParamType, std::nullptr_t>) {
        // Handle NULL values
        params.emplace_back("NULL");
      } else if constexpr (std::is_same_v<ParamType, std::string> ||
                           std::is_same_v<ParamType, const char*> ||
                           std::is_same_v<ParamType, std::string_view>) {
        // String types
        params.emplace_back(std::string(param));
      } else if constexpr (std::is_arithmetic_v<ParamType>) {
        // Numeric/boolean types: typed, sent over the binary protocol
        params.push_back(relx::make_bind_param(param, std::to_string(param)));
      } else {
        // Other types, try to use stream conversion
        std::ostringstream ss;
        ss << param;
        params.emplace_back(ss.str());
      }
    };

    // Add each parameter to the vector
    (add_param(std::forward<Args>(args)), ...);

    return execute_raw(sql, params);
  }

  /// @brief Check if the connection is open
  /// @return True if connected, false otherwise
  bool is_connected() const override;

  /// @brief Begin a new transaction with specified isolation level
  /// @param isolation_level The isolation level for the transaction
  /// @return Result indicating success or failure
  ConnectionResult<void> begin_transaction(
      IsolationLevel isolation_level = IsolationLevel::ReadCommitted) override;

  /// @brief Commit the current transaction
  /// @return Result indicating success or failure
  ConnectionResult<void> commit_transaction() override;

  /// @brief Rollback the current transaction
  /// @return Result indicating success or failure
  ConnectionResult<void> rollback_transaction() override;

  /// @brief Check if a transaction is currently active
  /// @return True if a transaction is active, false otherwise
  bool in_transaction() const override;

  /// @brief Create a prepared statement
  /// @param name The name of the prepared statement
  /// @param sql The SQL query text (? placeholders are converted to $n)
  /// @param param_count The number of parameters in the statement
  /// @return A new prepared statement, or the error PQprepare reported
  ConnectionResult<std::unique_ptr<PostgreSQLStatement>> prepare_statement(const std::string& name,
                                                                           const std::string& sql,
                                                                           int param_count);

  /// @brief Execute a previously prepared statement via PQexecPrepared
  /// @param statement_name The name the statement was prepared under
  /// @param params Text-format parameter values; std::nullopt binds SQL NULL
  /// @return Result containing the query results or an error
  ConnectionResult<result::ResultSet> execute_prepared(
      const std::string& statement_name, const std::vector<std::optional<std::string>>& params);

  /// @brief Get direct access to the PostgreSQL connection
  /// @return The PGconn pointer
  PGconn* get_pg_conn() { return pg_conn_; }

  /// @brief Convert SQL with ? placeholders to PostgreSQL's $n format
  /// @param sql SQL query with ? placeholders
  /// @return Converted SQL with $1, $2, etc. placeholders
  static std::string convert_placeholders(const std::string& sql);

private:
  std::string connection_string_;
  PGconn* pg_conn_ = nullptr;
  bool is_connected_ = false;
  bool in_transaction_ = false;

  /// @brief Helper method to handle PGresult and convert to ConnectionResult
  /// @param result PGresult pointer to process
  /// @param expected_status Expected status code (or -1 to ignore)
  /// @return ConnectionResult with error or success
  ConnectionResult<PGresult*> handle_pg_result(PGresult* result, int expected_status = -1);

  /// @brief Shared implementation of execute_raw / execute_raw_binary_result
  ConnectionResult<result::ResultSet> execute_params_internal(const std::string& sql,
                                                              const std::vector<bind_param>& params,
                                                              bool binary_results);
};

}  // namespace relx::connection