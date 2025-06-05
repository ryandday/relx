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
    auto result_set_output = co_await execute(query);
    if (!result_set_output) {
      co_return std::unexpected(result_set_output.error());
    }

    const auto& result_set = *result_set_output;

    if (result_set.empty()) {
      co_return std::unexpected(ConnectionError{.message = "No results found", .error_code = -1});
    }

    // Create an instance of T
    T obj{};

    // Get the first row of results
    const auto& row = result_set.at(0);

    // Use Boost.PFR to get the tuple type that matches our struct
    auto structure_tie = boost::pfr::structure_tie(obj);

    // Make sure the number of columns matches the number of fields in the struct
    if (result_set.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
      std::stringstream ss;
      for (const auto& param : query.bind_params()) {
        ss << param << ", ";
      }
      co_return std::unexpected(ConnectionError{
          .message = "Column count does not match struct field count, " +
                     std::to_string(result_set.column_count()) +
                     " != " + std::to_string(boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) +
                     " for struct " + typeid(T).name() + " and query " + query.to_sql() +
                     " with params " + ss.str(),
          .error_code = -1});
    }

    // Convert each value in the result row to the appropriate type in the struct
    try {
      // Create a vector of values from the row
      std::vector<std::string> values;
      for (size_t i = 0; i < result_set.column_count(); ++i) {
        auto cell_result = row.get_cell(i);
        if (!cell_result) {
          co_return std::unexpected(
              ConnectionError{.message = "Failed to get cell value: " + cell_result.error().message,
                              .error_code = -1});
        }
        values.push_back((*cell_result)->raw_value());
      }

      // Apply tuple assignment from the row values to the struct fields
      relx::connection::map_row_to_tuple(structure_tie, values);
    } catch (const std::exception& e) {
      co_return std::unexpected(
          ConnectionError{.message = std::string("Failed to convert result to struct: ") + e.what(),
                          .error_code = -1});
    }

    co_return obj;
  }

  /// @brief Execute a query and map results to a vector of user-defined types asynchronously
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Awaitable that resolves with a vector of mapped data
  template <typename T, query::SqlExpr Query>
  boost::asio::awaitable<ConnectionResult<std::vector<T>>> execute_many(const Query& query) {
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

    // Make sure the number of columns matches the number of fields in the struct
    if (result_set.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
      std::stringstream ss;
      for (const auto& param : query.bind_params()) {
        ss << param << ", ";
      }
      co_return std::unexpected(ConnectionError{
          .message = "Column count does not match struct field count, " +
                     std::to_string(result_set.column_count()) +
                     " != " + std::to_string(boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) +
                     " for struct " + typeid(T).name() + " and query " + query.to_sql() +
                     " with params " + ss.str(),
          .error_code = -1});
    }

    // Process each row
    for (size_t row_idx = 0; row_idx < result_set.size(); ++row_idx) {
      const auto& row = result_set.at(row_idx);
      T obj{};
      auto structure_tie = boost::pfr::structure_tie(obj);

      try {
        // Create a vector of values from the row
        std::vector<std::string> values;
        for (size_t i = 0; i < result_set.column_count(); ++i) {
          auto cell_result = row.get_cell(i);
          if (!cell_result) {
            co_return std::unexpected(ConnectionError{.message = "Failed to get cell value: " +
                                                                 cell_result.error().message,
                                                      .error_code = -1});
          }
          values.push_back((*cell_result)->raw_value());
        }

        relx::connection::map_row_to_tuple(structure_tie, values);
        objects.push_back(std::move(obj));
      } catch (const std::exception& e) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Failed to convert result to struct: ") + e.what(),
            .error_code = -1});
      }
    }

    co_return objects;
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

private:
  boost::asio::io_context& io_context_;
  std::string connection_string_;
  std::unique_ptr<pgsql_async_wrapper::Connection> async_conn_;
  bool is_connected_ = false;
  bool in_transaction_ = false;

  /// @brief Helper method to convert pgsql_async_wrapper::result to relx::result::ResultSet
  ConnectionResult<result::ResultSet> convert_result(const pgsql_async_wrapper::Result& pg_result);

  /// @brief Convert SQL with ? placeholders to PostgreSQL's $n format
  /// @param sql SQL query with ? placeholders
  /// @return Converted SQL with $1, $2, etc. placeholders
  std::string convert_placeholders(const std::string& sql);
};

}  // namespace relx::connection
