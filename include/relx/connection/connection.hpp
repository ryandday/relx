#pragma once

#include "../query/core.hpp"
#include "../results/result.hpp"
#include "meta.hpp"

#include <expected>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <boost/pfr.hpp>

namespace relx {
namespace connection {

/// @brief Error type for database connection operations
struct ConnectionError {
  std::string message;
  int error_code = 0;
};

/// @brief Type alias for result of connection operations
template <typename T>
using ConnectionResult = std::expected<T, ConnectionError>;

/// @brief Transaction isolation levels
enum class IsolationLevel {
  ReadUncommitted,  ///< Allows dirty reads
  ReadCommitted,    ///< Prevents dirty reads
  RepeatableRead,   ///< Prevents non-repeatable reads
  Serializable      ///< Highest isolation level, prevents phantom reads
};

/// @brief Basic parameters for a PostgreSQL connection
struct PostgreSQLConnectionParams {
  std::string host = "localhost";
  uint16_t port = 5432;
  std::string dbname;
  std::string user;
  std::string password;
  std::string application_name;
  int connect_timeout = 30;  // seconds

  // Optional parameters
  std::string ssl_mode;  // disable, require, verify-ca, verify-full
  std::string ssl_cert;
  std::string ssl_key;
  std::string ssl_root_cert;

  /// @brief Convert parameters to a PostgreSQL connection string
  /// @return Connection string in libpq format (e.g., "host=localhost port=5432 dbname=mydb...")
  std::string to_connection_string() const {
    std::ostringstream conn_str;

    if (!host.empty()) conn_str << "host=" << host << " ";
    conn_str << "port=" << port << " ";
    if (!dbname.empty()) conn_str << "dbname=" << dbname << " ";
    if (!user.empty()) conn_str << "user=" << user << " ";
    if (!password.empty()) conn_str << "password=" << password << " ";
    if (!application_name.empty()) conn_str << "application_name=" << application_name << " ";
    conn_str << "connect_timeout=" << connect_timeout << " ";

    // Optional SSL parameters
    if (!ssl_mode.empty()) conn_str << "sslmode=" << ssl_mode << " ";
    if (!ssl_cert.empty()) conn_str << "sslcert=" << ssl_cert << " ";
    if (!ssl_key.empty()) conn_str << "sslkey=" << ssl_key << " ";
    if (!ssl_root_cert.empty()) conn_str << "sslrootcert=" << ssl_root_cert << " ";

    std::string result = conn_str.str();
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();  // Remove trailing space
    }

    return result;
  }
};

/// @brief Abstract base class for database connections
class Connection {
public:
  /// @brief Virtual destructor
  virtual ~Connection() = default;

  /// @brief Connect to the database
  /// @return Result indicating success or failure
  [[nodiscard]] virtual ConnectionResult<void> connect() = 0;

  /// @brief Disconnect from the database
  /// @return Result indicating success or failure
  [[nodiscard]] virtual ConnectionResult<void> disconnect() = 0;

  /// @brief Execute a raw SQL query with parameters
  /// @param sql The SQL query string
  /// @param params Vector of parameter values
  /// @return Result containing the query results or an error
  [[nodiscard]]
  virtual ConnectionResult<result::ResultSet> execute_raw(
      const std::string& sql, const std::vector<std::string>& params = {}) = 0;

  /// @brief Execute a query expression
  /// @param query The query expression to execute
  /// @return Result containing the query results or an error
  template <query::SqlExpr Query>
  [[nodiscard]]
  ConnectionResult<result::ResultSet> execute(const Query& query) {
    std::string sql = query.to_sql();
    std::vector<std::string> params = query.bind_params();
    return execute_raw(sql, params);
  }

  /// @brief Execute a query and map results to a user-defined type using Boost.PFR
  /// @note The struct must be an aggregate type (has no virtual functions or private members)
  /// @note The struct should have public members in the same order as the columns in the result set
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Result containing the mapped user-defined type or an error
  template <typename T, query::SqlExpr Query>
  [[nodiscard]]
  ConnectionResult<T> execute(const Query& query) {
    auto result = execute(query);
    if (!result) {
      return std::unexpected(result.error());
    }

    const auto& resultSet = *result;
    if (resultSet.empty()) {
      return std::unexpected(ConnectionError{.message = "No results found"});
    }

    // Create an instance of T
    T obj{};

    // Get the first row of results
    const auto& row = resultSet.at(0);

    // Use Boost.PFR to get the tuple type that matches our struct
    auto structure_tie = boost::pfr::structure_tie(obj);

    // Make sure the number of columns matches the number of fields in the struct
    // TODO What if struct has less fields than columns? Why is it working?
    if (resultSet.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
      std::stringstream ss;
      for (const auto& param : query.bind_params()) {
        ss << param << ", ";
      }
      return std::unexpected(ConnectionError{
          .message = "Column count does not match struct field count, " +
                     std::to_string(resultSet.column_count()) +
                     " != " + std::to_string(boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) +
                     " for struct " + typeid(T).name() + " and query " + query.to_sql() +
                     " with params " + ss.str(),
          .error_code = -1});
    }

    // Convert each value in the result row to the appropriate type in the struct
    try {
      // Create a vector of values from the row
      std::vector<std::string> values;
      for (size_t i = 0; i < resultSet.column_count(); ++i) {
        auto cell_result = row.get_cell(i);
        if (!cell_result) {
          return std::unexpected(
              ConnectionError{.message = "Failed to get cell value: " + cell_result.error().message,
                              .error_code = -1});
        }
        values.push_back((*cell_result)->raw_value());
      }

      // Apply tuple assignment from the row values to the struct fields
      relx::connection::map_row_to_tuple(structure_tie, values);
    } catch (const std::exception& e) {
      return std::unexpected(
          ConnectionError{.message = std::string("Failed to convert result to struct: ") + e.what(),
                          .error_code = -1});
    }

    return obj;
  }

  /// @brief Execute a query and map results to a vector of user-defined types
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Result containing a vector of mapped user-defined types or an error
  template <typename T, query::SqlExpr Query>
  [[nodiscard]]
  ConnectionResult<std::vector<T>> execute_many(const Query& query) {
    auto result = execute(query);
    if (!result) {
      return std::unexpected(result.error());
    }

    const auto& resultSet = *result;
    std::vector<T> objects;
    objects.reserve(resultSet.size());

    // Check if we have at least one row to determine column count
    if (resultSet.empty()) {
      return objects;  // Return empty vector
    }

    // Make sure the number of columns matches the number of fields in the struct
    // TODO
    if (resultSet.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
      std::stringstream ss;
      for (const auto& param : query.bind_params()) {
        ss << param << ", ";
      }
      return std::unexpected(ConnectionError{
          .message = "Column count does not match struct field count, " +
                     std::to_string(resultSet.column_count()) +
                     " != " + std::to_string(boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) +
                     " for struct " + typeid(T).name() + " and query " + query.to_sql() +
                     " with params " + ss.str(),
          .error_code = -1});
    }

    // Process each row
    for (size_t row_idx = 0; row_idx < resultSet.size(); ++row_idx) {
      const auto& row = resultSet.at(row_idx);
      T obj{};
      auto structure_tie = boost::pfr::structure_tie(obj);

      try {
        // Create a vector of values from the row
        std::vector<std::string> values;
        for (size_t i = 0; i < resultSet.column_count(); ++i) {
          auto cell_result = row.get_cell(i);
          if (!cell_result) {
            return std::unexpected(ConnectionError{.message = "Failed to get cell value: " +
                                                              cell_result.error().message,
                                                   .error_code = -1});
          }
          values.push_back((*cell_result)->raw_value());
        }

        relx::connection::map_row_to_tuple(structure_tie, values);
        objects.push_back(std::move(obj));
      } catch (const std::exception& e) {
        return std::unexpected(ConnectionError{
            .message = std::string("Failed to convert result to struct: ") + e.what(),
            .error_code = -1});
      }
    }

    return objects;
  }

  /// @brief Check if the connection is open
  /// @return True if connected, false otherwise
  virtual bool is_connected() const = 0;

  /// @brief Begin a new transaction
  /// @param isolation_level The isolation level for the transaction
  /// @return Result indicating success or failure
  [[nodiscard]]
  virtual ConnectionResult<void> begin_transaction(
      IsolationLevel isolation_level = IsolationLevel::ReadCommitted) = 0;

  /// @brief Commit the current transaction
  /// @return Result indicating success or failure
  [[nodiscard]]
  virtual ConnectionResult<void> commit_transaction() = 0;

  /// @brief Rollback the current transaction
  /// @return Result indicating success or failure
  [[nodiscard]]
  virtual ConnectionResult<void> rollback_transaction() = 0;

  /// @brief Check if a transaction is currently active
  /// @return True if a transaction is active, false otherwise
  virtual bool in_transaction() const = 0;

private:
};

}  // namespace relx::connection

// Convenient imports from the connection namespace
using relx::connection::Connection;
using relx::connection::ConnectionError;
using relx::connection::ConnectionResult;
using relx::connection::IsolationLevel;
}  // namespace relx