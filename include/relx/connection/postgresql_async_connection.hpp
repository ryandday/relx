#pragma once

#include "../connection/pgsql_async_wrapper.hpp"
#include "../results/result.hpp"
#include "connection.hpp"
#include "meta.hpp"

#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace relx::connection {

/// @brief Asynchronous PostgreSQL implementation of the Connection interface
class PostgreSQLAsyncConnection {
public:
  /// @brief Constructor with connection parameters and io_context
  /// @param io_context Boost.Asio IO context for async operations
  /// @param connection_string PostgreSQL connection string (e.g. "host=localhost port=5432
  /// dbname=mydb user=postgres password=password")
  explicit PostgreSQLAsyncConnection(boost::asio::io_context& io_context,
                                     std::string connection_string);

  /// @brief Constructor with structured connection parameters and io_context
  /// @param io_context Boost.Asio IO context for async operations
  /// @param params PostgreSQL connection parameters
  explicit PostgreSQLAsyncConnection(boost::asio::io_context& io_context,
                                     const PostgreSQLConnectionParams& params);

  /// @brief Destructor that ensures proper cleanup
  ~PostgreSQLAsyncConnection();

  // Delete copy constructor and assignment operator
  PostgreSQLAsyncConnection(const PostgreSQLAsyncConnection&) = delete;
  PostgreSQLAsyncConnection& operator=(const PostgreSQLAsyncConnection&) = delete;

  // Allow move operations
  PostgreSQLAsyncConnection(PostgreSQLAsyncConnection&&) noexcept;
  PostgreSQLAsyncConnection& operator=(PostgreSQLAsyncConnection&&) noexcept;

  /// @brief Check if the connection is open
  /// @return True if connected, false otherwise
  bool is_connected() const;

  /// @brief Check if a transaction is currently active
  /// @return True if a transaction is active, false otherwise
  bool in_transaction() const;

  //====================================================================
  // The following methods provide the asynchronous awaitable interface
  //====================================================================

  /// @brief Connect to the PostgreSQL database asynchronously
  /// @return Awaitable that resolves when connection is established
  boost::asio::awaitable<ConnectionResult<void>> connect();

  /// @brief Disconnect from the PostgreSQL database asynchronously
  /// @return Awaitable that resolves when disconnection is complete
  boost::asio::awaitable<ConnectionResult<void>> disconnect();

  /// @brief Execute a raw SQL query with parameters asynchronously
  /// @param sql The SQL query string
  /// @param params Vector of parameter values
  /// @return Awaitable that resolves with the query results
  boost::asio::awaitable<ConnectionResult<result::ResultSet>> execute_raw(
      std::string sql, std::vector<std::string> params = {});

  /// @brief Execute a query expression asynchronously
  /// @param query The query expression to execute
  /// @return Awaitable that resolves with the query results
  template <query::SqlExpr Query>
  boost::asio::awaitable<ConnectionResult<result::ResultSet>> execute(Query query) {
    std::string sql = query.to_sql();
    std::vector<std::string> params = query.bind_params();
    return execute_raw(sql, params);
  }

  /// @brief Execute a query and map results to a user-defined type asynchronously
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Awaitable that resolves with the mapped data
  template <typename T, query::SqlExpr Query>
  boost::asio::awaitable<ConnectionResult<T>> execute(Query query) {
    assert_struct_covers_select_list<T, Query>();
    auto result_set_output = co_await execute(query);
    if (!result_set_output) {
      co_return std::unexpected(result_set_output.error());
    }

    const auto& result_set = *result_set_output;

    if (result_set.empty()) {
      co_return std::unexpected(ConnectionError{.message = "No results found", .error_code = -1});
    }

    auto mapped = map_row_to_struct<T>(result_set.at(0));
    if (!mapped) {
      co_return std::unexpected(ConnectionError{
          .message = "Failed to convert result to struct: " + mapped.error(), .error_code = -1});
    }

    co_return *mapped;
  }

  /// @brief Execute a query and map results to a vector of user-defined types asynchronously
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Awaitable that resolves with a vector of mapped data
  template <typename T, query::SqlExpr Query>
  boost::asio::awaitable<ConnectionResult<std::vector<T>>> execute_many(const Query& query) {
    assert_struct_covers_select_list<T, Query>();
    auto result_set_output = co_await execute(query);
    if (!result_set_output) {
      co_return std::unexpected(result_set_output.error());
    }

    const auto& result_set = *result_set_output;

    std::vector<T> objects;
    objects.reserve(result_set.size());

    // Check if we have at least one row to determine column count
    if (result_set.empty()) {
      co_return objects;  // Return empty vector
    }

    // Process each row
    for (size_t row_idx = 0; row_idx < result_set.size(); ++row_idx) {
      auto mapped = map_row_to_struct<T>(result_set.at(row_idx));
      if (!mapped) {
        co_return std::unexpected(ConnectionError{
            .message = "Failed to convert result to struct: " + mapped.error(), .error_code = -1});
      }
      objects.push_back(std::move(*mapped));
    }

    co_return objects;
  }

  /// @brief Execute a select query asynchronously, mapping rows onto its synthesized
  /// row type (see query/row_type.hpp)
  /// @param query The select query to execute
  /// @return Awaitable resolving with a vector of synthesized rows
  template <query::RowSynthesizable Query>
  boost::asio::awaitable<ConnectionResult<std::vector<query::row_type_for<Query>>>> fetch_all(
      const Query& query) {
    co_return co_await execute_many<query::row_type_for<Query>>(query);
  }

  /// @brief Execute a select query expected to yield one row asynchronously, mapped
  /// onto its synthesized row type
  /// @param query The select query to execute
  /// @return Awaitable resolving with the synthesized row
  template <query::RowSynthesizable Query>
  boost::asio::awaitable<ConnectionResult<query::row_type_for<Query>>> fetch_one(
      const Query& query) {
    co_return co_await execute<query::row_type_for<Query>>(query);
  }

  /// @brief Begin a new transaction asynchronously
  /// @param isolation_level The isolation level for the transaction
  /// @return Awaitable that resolves when transaction begins
  boost::asio::awaitable<ConnectionResult<void>> begin_transaction(
      IsolationLevel isolation_level = IsolationLevel::ReadCommitted);

  /// @brief Commit the current transaction asynchronously
  /// @return Awaitable that resolves when transaction is committed
  boost::asio::awaitable<ConnectionResult<void>> commit_transaction();

  /// @brief Rollback the current transaction asynchronously
  /// @return Awaitable that resolves when transaction is rolled back
  boost::asio::awaitable<ConnectionResult<void>> rollback_transaction();

  /// @brief Get the underlying async connection wrapper
  /// @return Reference to the async wrapper connection
  pgsql_async_wrapper::Connection& get_async_conn() { return *async_conn_; }

  /// Get the IO context associated with this connection
  boost::asio::io_context& get_io_context() const { return io_context_; }

  /// @brief Reset connection state after streaming operations
  /// @return Awaitable that resolves when the connection is ready for new commands
  boost::asio::awaitable<ConnectionResult<void>> reset_connection_state();

  /// @brief Reset connection state synchronously (for use in destructors)
  /// @return True if reset was successful, false otherwise
  /// @details This is a non-blocking version that can be called from destructors
  /// when async streaming result sets go out of scope before completion
  bool reset_connection_state_sync();

private:
  boost::asio::io_context& io_context_;
  std::string connection_string_;
  std::unique_ptr<pgsql_async_wrapper::Connection> async_conn_;
  bool is_connected_ = false;
  bool in_transaction_ = false;

  /// @brief Helper method to convert pgsql_async_wrapper::result to relx::result::ResultSet
  static ConnectionResult<result::ResultSet> convert_result(
      const pgsql_async_wrapper::Result& pg_result);

  /// @brief Convert SQL with ? placeholders to PostgreSQL's $n format
  /// @param sql SQL query with ? placeholders
  /// @return Converted SQL with $1, $2, etc. placeholders
  static std::string convert_placeholders(const std::string& sql);
};

}  // namespace relx::connection
