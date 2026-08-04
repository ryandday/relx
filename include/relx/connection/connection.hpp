#pragma once

#include "../query/core.hpp"
#include "../query/row_type.hpp"
#include "../results/result.hpp"
#include "meta.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

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

namespace detail {

/// @brief Compile-time name of a select-list element, when it has one
template <typename E>
consteval std::optional<std::string_view> select_element_name() {
  if constexpr (requires { typename E::column_type; }) {
    return std::string_view(E::column_type::name);
  } else if constexpr (requires { std::string_view(E::alias_name); }) {
    return std::string_view(E::alias_name);
  } else {
    return std::nullopt;
  }
}

/// @brief Comma-separated list of selected column names with no matching field in T
template <typename T, typename Tuple>
struct uncovered_columns;

template <typename T, typename... Es>
struct uncovered_columns<T, std::tuple<Es...>> {
  static consteval std::string get() {
    std::string out;
    auto add = [&out](std::optional<std::string_view> name) {
      if (name && !refl::has_field_named<T>(*name)) {
        out += out.empty() ? "'" : ", '";
        out += *name;
        out += '\'';
      }
    };
    (add(select_element_name<Es>()), ...);
    return out;
  }
};

}  // namespace detail

/// @brief Compile-time coverage check for typed queries: every selected column must have
/// a matching field in the result struct. The struct may have additional fields - they
/// are left default-initialized, since selecting a subset expresses that the other
/// columns are not wanted. No-op for queries without a compile-time select list.
template <typename T, typename Query>
consteval void assert_struct_covers_select_list() {
  if constexpr (requires { typename std::remove_cvref_t<Query>::columns_type; }) {
    using Columns = typename std::remove_cvref_t<Query>::columns_type;
    using Struct = std::remove_cvref_t<T>;
    static_assert(detail::uncovered_columns<Struct, Columns>::get().empty(),
                  std::string("query selects column(s) ") +
                      detail::uncovered_columns<Struct, Columns>::get() + " but result struct '" +
                      std::string(refl::type_name<Struct>()) +
                      "' has no field(s) with those names");
  }
}

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

    if (!host.empty()) {
      conn_str << "host=" << host << " ";
    }
    conn_str << "port=" << port << " ";
    if (!dbname.empty()) {
      conn_str << "dbname=" << dbname << " ";
    }
    if (!user.empty()) {
      conn_str << "user=" << user << " ";
    }
    if (!password.empty()) {
      conn_str << "password=" << password << " ";
    }
    if (!application_name.empty()) {
      conn_str << "application_name=" << application_name << " ";
    }
    conn_str << "connect_timeout=" << connect_timeout << " ";

    // Optional SSL parameters
    if (!ssl_mode.empty()) {
      conn_str << "sslmode=" << ssl_mode << " ";
    }
    if (!ssl_cert.empty()) {
      conn_str << "sslcert=" << ssl_cert << " ";
    }
    if (!ssl_key.empty()) {
      conn_str << "sslkey=" << ssl_key << " ";
    }
    if (!ssl_root_cert.empty()) {
      conn_str << "sslrootcert=" << ssl_root_cert << " ";
    }

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

  /// @brief Execute a query and map results to a user-defined type using reflection
  /// @note The struct must be an aggregate type (has no virtual functions or private members)
  /// @note Struct fields are matched to result columns by name, falling back to position
  /// for fields whose name does not appear in the result set
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Result containing the mapped user-defined type or an error
  template <typename T, query::SqlExpr Query>
  [[nodiscard]]
  ConnectionResult<T> execute(const Query& query) {
    assert_struct_covers_select_list<T, Query>();
    auto result = execute(query);
    if (!result) {
      return std::unexpected(result.error());
    }

    const auto& result_set = *result;
    if (result_set.empty()) {
      return std::unexpected(ConnectionError{.message = "No results found"});
    }

    if (auto consumed = verify_result_columns_consumed<T>(result_set.column_names(),
                                                          result_set.column_count());
        !consumed) {
      return std::unexpected(ConnectionError{.message = consumed.error(), .error_code = -1});
    }

    auto mapped = map_row_to_struct<T>(result_set.at(0));
    if (!mapped) {
      return std::unexpected(ConnectionError{
          .message = "Failed to convert result to struct: " + mapped.error(), .error_code = -1});
    }

    return *mapped;
  }

  /// @brief Execute a query and map results to a vector of user-defined types
  /// @tparam T The user-defined type to map results to
  /// @tparam Query The query expression type
  /// @param query The query expression to execute
  /// @return Result containing a vector of mapped user-defined types or an error
  template <typename T, query::SqlExpr Query>
  [[nodiscard]]
  ConnectionResult<std::vector<T>> execute_many(const Query& query) {
    assert_struct_covers_select_list<T, Query>();
    auto result = execute(query);
    if (!result) {
      return std::unexpected(result.error());
    }

    const auto& result_set = *result;
    std::vector<T> objects;
    objects.reserve(result_set.size());

    // Check if we have at least one row to determine column count
    if (result_set.empty()) {
      return objects;  // Return empty vector
    }

    if (auto consumed = verify_result_columns_consumed<T>(result_set.column_names(),
                                                          result_set.column_count());
        !consumed) {
      return std::unexpected(ConnectionError{.message = consumed.error(), .error_code = -1});
    }

    // Process each row
    for (size_t row_idx = 0; row_idx < result_set.size(); ++row_idx) {
      auto mapped = map_row_to_struct<T>(result_set.at(row_idx));
      if (!mapped) {
        return std::unexpected(ConnectionError{
            .message = "Failed to convert result to struct: " + mapped.error(), .error_code = -1});
      }
      objects.push_back(std::move(*mapped));
    }

    return objects;
  }

  /// @brief Execute a select query and map rows onto its synthesized row type
  /// @details The row struct is generated from the select list itself (see
  /// query/row_type.hpp), so no hand-written DTO is needed and a select-list/row
  /// mismatch cannot happen
  /// @param query The select query to execute
  /// @return Result containing a vector of synthesized rows or an error
  template <query::RowSynthesizable Query>
  [[nodiscard]]
  ConnectionResult<std::vector<query::row_type_for<Query>>> fetch_all(const Query& query) {
    return execute_many<query::row_type_for<Query>>(query);
  }

  /// @brief Execute a select query expected to yield one row, mapped onto its
  /// synthesized row type
  /// @param query The select query to execute
  /// @return Result containing the synthesized row or an error
  template <query::RowSynthesizable Query>
  [[nodiscard]]
  ConnectionResult<query::row_type_for<Query>> fetch_one(const Query& query) {
    return execute<query::row_type_for<Query>>(query);
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

}  // namespace connection

// Convenient imports from the connection namespace
using relx::connection::Connection;
using relx::connection::ConnectionError;
using relx::connection::ConnectionResult;
using relx::connection::IsolationLevel;
}  // namespace relx