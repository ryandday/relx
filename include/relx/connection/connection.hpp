#pragma once

#include "../query/core.hpp"
#include "../query/row_type.hpp"
#include "../query/static_shape.hpp"
#include "../results/result.hpp"
#include "meta.hpp"

#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <boost/uuid/uuid.hpp>

namespace relx {
namespace connection {

/// @brief Error type for database connection operations
/// @details For errors reported by the server, sql_state carries the five-character
/// SQLSTATE code (e.g. "23505") and the diagnostic fields whatever the server attached;
/// all of them are empty for client-side errors (not connected, bad parameters, ...)
struct ConnectionError {
  std::string message;
  int error_code = 0;
  std::string sql_state;
  std::string detail;
  std::string hint;
  std::string constraint_name;

  /// @brief True if this is a unique-constraint violation (SQLSTATE 23505)
  bool is_duplicate_key_error() const { return sql_state == "23505"; }
  /// @brief True if this is a foreign-key violation (SQLSTATE 23503)
  bool is_foreign_key_violation() const { return sql_state == "23503"; }
  /// @brief True if this is a check-constraint violation (SQLSTATE 23514)
  bool is_check_constraint_violation() const { return sql_state == "23514"; }
  /// @brief True if this is a not-null violation (SQLSTATE 23502)
  bool is_not_null_violation() const { return sql_state == "23502"; }
  /// @brief True if the transaction must be retried: serialization failure (SQLSTATE 40001)
  bool is_serialization_failure() const { return sql_state == "40001"; }
  /// @brief True if the transaction must be retried: deadlock detected (SQLSTATE 40P01)
  bool is_deadlock() const { return sql_state == "40P01"; }
};

/// @brief Type alias for result of connection operations
template <typename T>
using ConnectionResult = std::expected<T, ConnectionError>;

namespace detail {

/// @brief Compile-time name of a select-list element, when it has one. A whole-table
/// element is named after its table (the synthesized nested row member's name).
template <typename E>
consteval std::optional<std::string_view> select_element_name() {
  if constexpr (TableSelectElement<E>) {
    return std::string_view(E::table_type::table_name);
  } else if constexpr (requires { typename E::column_type; }) {
    return std::string_view(E::column_type::name);
  } else if constexpr (requires { std::string_view(E::alias_name); }) {
    return std::string_view(E::alias_name);
  } else {
    return std::nullopt;
  }
}

/// @brief Whether the query's result columns include a whole-table element, which
/// switches result mapping to the positional grouped mapper
template <typename Query>
consteval bool query_selects_whole_tables() {
  using Columns = query::result_columns_t<Query>;
  if constexpr (std::is_void_v<Columns>) {
    return false;
  } else {
    return []<typename... Es>(std::type_identity<std::tuple<Es...>>) {
      return (TableSelectElement<Es> || ...);
    }(std::type_identity<Columns>{});
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

/// @brief Whether a select-list element's C++ type can be decoded from PostgreSQL's
/// binary result format (see sql_utils::process_postgresql_result_binary)
template <typename T>
consteval bool binary_decodable_value_type() {
  using Stripped = std::remove_cvref_t<T>;
  if constexpr (requires {
                  typename Stripped::value_type;
                  requires std::same_as<Stripped, std::optional<typename Stripped::value_type>>;
                }) {
    return binary_decodable_value_type<typename Stripped::value_type>();
  } else {
    return std::is_same_v<Stripped, bool> || std::is_integral_v<Stripped> ||
           std::is_floating_point_v<Stripped> || std::is_same_v<Stripped, std::string> ||
           std::is_enum_v<Stripped> ||
           std::is_same_v<Stripped, std::chrono::system_clock::time_point> ||
           std::is_same_v<Stripped, std::chrono::year_month_day> ||
           std::is_same_v<Stripped, boost::uuids::uuid>;
  }
}

template <typename E>
consteval bool binary_decodable_element() {
  if constexpr (requires { typename E::value_type; }) {
    return binary_decodable_value_type<typename E::value_type>();
  } else {
    return false;
  }
}

/// @brief Whether every element of the query's compile-time result columns (select
/// list, or RETURNING list for DML) maps to a binary-decodable type. Queries without
/// result columns (raw SQL) or with elements of unknown type (e.g. aggregates without
/// value_type, whole-table elements) use the text protocol.
template <typename Query>
consteval bool query_supports_binary_results() {
  using Columns = query::result_columns_t<Query>;
  if constexpr (std::is_void_v<Columns>) {
    return false;
  } else {
    return []<typename... Es>(std::type_identity<std::tuple<Es...>>) {
      return (binary_decodable_element<Es>() && ...);
    }(std::type_identity<Columns>{});
  }
}

}  // namespace detail

/// @brief Compile-time coverage check for typed queries: every result column (selected
/// or RETURNING) must have a matching field in the result struct. The struct may have
/// additional fields - they are left default-initialized, since selecting a subset
/// expresses that the other columns are not wanted. No-op for queries without
/// compile-time result columns.
template <typename T, typename Query>
consteval void assert_struct_covers_select_list() {
  if constexpr (!std::is_void_v<query::result_columns_t<Query>>) {
    using Columns = query::result_columns_t<Query>;
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
  /// @details Values are single-quoted with `\`/`'` escaped per libpq's conninfo
  /// grammar, so passwords (or any value) containing spaces or quotes survive.
  std::string to_connection_string() const {
    // libpq conninfo values may be single-quoted; backslash and single quote are
    // escaped with a backslash
    const auto quoted = [](const std::string& value) {
      std::string out;
      out.reserve(value.size() + 2);
      out += '\'';
      for (const char c : value) {
        if (c == '\\' || c == '\'') {
          out += '\\';
        }
        out += c;
      }
      out += '\'';
      return out;
    };

    std::ostringstream conn_str;

    if (!host.empty()) {
      conn_str << "host=" << quoted(host) << " ";
    }
    conn_str << "port=" << port << " ";
    if (!dbname.empty()) {
      conn_str << "dbname=" << quoted(dbname) << " ";
    }
    if (!user.empty()) {
      conn_str << "user=" << quoted(user) << " ";
    }
    if (!password.empty()) {
      conn_str << "password=" << quoted(password) << " ";
    }
    if (!application_name.empty()) {
      conn_str << "application_name=" << quoted(application_name) << " ";
    }
    conn_str << "connect_timeout=" << connect_timeout << " ";

    // Optional SSL parameters
    if (!ssl_mode.empty()) {
      conn_str << "sslmode=" << quoted(ssl_mode) << " ";
    }
    if (!ssl_cert.empty()) {
      conn_str << "sslcert=" << quoted(ssl_cert) << " ";
    }
    if (!ssl_key.empty()) {
      conn_str << "sslkey=" << quoted(ssl_key) << " ";
    }
    if (!ssl_root_cert.empty()) {
      conn_str << "sslrootcert=" << quoted(ssl_root_cert) << " ";
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
  /// @param params Parameter values; params carrying a sql_kind tag are sent typed over
  /// the binary protocol where the connection supports it, plain text otherwise
  /// @return Result containing the query results or an error
  [[nodiscard]]
  virtual ConnectionResult<result::ResultSet> execute_raw(
      const std::string& sql, const std::vector<bind_param>& params = {}) = 0;

  /// @brief Execute a raw SQL query requesting results in binary format, decoded back
  /// into canonical text cells. Connections without binary support fall back to the
  /// text protocol. Only call for single-statement queries whose result columns are
  /// binary-decodable (bool/int/float/text/enum) - typed execute() gates this
  /// automatically.
  [[nodiscard]]
  virtual ConnectionResult<result::ResultSet> execute_raw_binary_result(
      const std::string& sql, const std::vector<bind_param>& params = {}) {
    return execute_raw(sql, params);
  }

  /// @brief Execute a query expression. Typed select queries whose select list is
  /// binary-decodable use libpq's binary result format (no server-side text
  /// formatting, no text parsing of numerics); everything else uses the text protocol.
  /// Static-shaped queries (SQL text fully determined by the type - see
  /// has_static_shape_v) render their SQL once per query type instead of on every
  /// execution.
  /// @param query The query expression to execute
  /// @return Result containing the query results or an error
  template <query::SqlExpr Query>
  [[nodiscard]]
  ConnectionResult<result::ResultSet> execute(const Query& query) {
    if constexpr (query::has_static_shape_v<std::remove_cvref_t<Query>>) {
      static const std::string sql = query.to_sql();  // once per query type
      return execute_with_sql(sql, query);
    } else {
      return execute_with_sql(query.to_sql(), query);
    }
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

    if constexpr (detail::query_selects_whole_tables<Query>()) {
      // Whole-table selects map positionally: result column names repeat across the
      // joined tables, so name-based mapping cannot apply
      auto mapped = grouped_row_mapper<T, query::result_columns_t<Query>>::map(result_set.at(0));
      if (!mapped) {
        return std::unexpected(ConnectionError{
            .message = "Failed to convert result to struct: " + mapped.error(), .error_code = -1});
      }
      return *mapped;
    } else {
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

    if constexpr (detail::query_selects_whole_tables<Query>()) {
      // Whole-table selects map positionally: result column names repeat across the
      // joined tables, so name-based mapping cannot apply
      for (size_t row_idx = 0; row_idx < result_set.size(); ++row_idx) {
        auto mapped = grouped_row_mapper<T, query::result_columns_t<Query>>::map(
            result_set.at(row_idx));
        if (!mapped) {
          return std::unexpected(
              ConnectionError{.message = "Failed to convert result to struct: " + mapped.error(),
                              .error_code = -1});
        }
        objects.push_back(std::move(*mapped));
      }
      return objects;
    } else {
      if (auto consumed = verify_result_columns_consumed<T>(result_set.column_names(),
                                                            result_set.column_count());
          !consumed) {
        return std::unexpected(ConnectionError{.message = consumed.error(), .error_code = -1});
      }

      // Process each row
      for (size_t row_idx = 0; row_idx < result_set.size(); ++row_idx) {
        auto mapped = map_row_to_struct<T>(result_set.at(row_idx));
        if (!mapped) {
          return std::unexpected(
              ConnectionError{.message = "Failed to convert result to struct: " + mapped.error(),
                              .error_code = -1});
        }
        objects.push_back(std::move(*mapped));
      }

      return objects;
    }
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

private:
  template <query::SqlExpr Query>
  ConnectionResult<result::ResultSet> execute_with_sql(const std::string& sql, const Query& query) {
    std::vector<bind_param> params = query.bind_params();
    if constexpr (detail::query_supports_binary_results<Query>()) {
      return execute_raw_binary_result(sql, params);
    } else {
      return execute_raw(sql, params);
    }
  }

public:
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