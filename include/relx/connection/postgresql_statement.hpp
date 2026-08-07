#pragma once

#include "postgresql_connection.hpp"

#include <format>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace relx::connection {

/// @brief Represents a prepared statement in PostgreSQL
class PostgreSQLStatement {
public:
  /// @brief Constructor
  /// @param connection The connection that owns this prepared statement
  /// @param name The name of the prepared statement
  /// @param sql The SQL query text
  /// @param param_count The number of parameters in the statement
  PostgreSQLStatement(PostgreSQLConnection& connection, std::string name, std::string sql,
                      int param_count);

  /// @brief Destructor that deallocates the prepared statement
  ~PostgreSQLStatement();

  // Delete copy operations
  PostgreSQLStatement(const PostgreSQLStatement&) = delete;
  PostgreSQLStatement& operator=(const PostgreSQLStatement&) = delete;

  // Allow move operations
  PostgreSQLStatement(PostgreSQLStatement&&) noexcept;
  PostgreSQLStatement& operator=(PostgreSQLStatement&&) noexcept;

  /// @brief Execute the prepared statement with parameters
  /// @param params Parameter values sent through the protocol (PQexecPrepared);
  /// std::nullopt binds SQL NULL
  /// @return Result containing the query results or an error
  ConnectionResult<result::ResultSet> execute(
      const std::vector<std::optional<std::string>>& params = {});

  /// @brief Execute the prepared statement with typed parameters
  /// @tparam Args The types of the parameters
  /// @param args The parameter values
  /// @return Result containing the query results or an error
  template <typename... Args>
  ConnectionResult<result::ResultSet> execute_typed(Args&&... args) {
    // Convert each parameter to its string representation
    std::vector<std::optional<std::string>> params;
    params.reserve(sizeof...(Args));

    // Helper to convert a parameter to string and add to vector
    auto add_param = [&params](auto&& param) {
      using ParamType = std::remove_cvref_t<decltype(param)>;

      if constexpr (std::is_same_v<ParamType, std::nullptr_t>) {
        params.push_back(std::nullopt);
      } else if constexpr (std::is_same_v<ParamType, std::string> ||
                           std::is_same_v<ParamType, const char*> ||
                           std::is_same_v<ParamType, std::string_view>) {
        params.emplace_back(std::string(param));
      } else if constexpr (std::is_same_v<ParamType, bool>) {
        params.emplace_back(param ? "t" : "f");
      } else if constexpr (std::is_arithmetic_v<ParamType>) {
        // std::format gives shortest round-trip form; std::to_string would truncate
        // floating point to 6 digits and follow the locale
        params.emplace_back(std::format("{}", param));
      } else {
        // Other types, try to use stream conversion
        std::ostringstream ss;
        ss << param;
        params.emplace_back(ss.str());
      }
    };

    // Add each parameter to the vector
    (add_param(std::forward<Args>(args)), ...);

    // Call the string-based execute with our converted parameters
    return execute(params);
  }

  /// @brief Get the name of the prepared statement
  /// @return The name of the prepared statement
  const std::string& name() const { return name_; }

  /// @brief Get the SQL query text
  /// @return The SQL query text
  const std::string& sql() const { return sql_; }

  /// @brief Get the number of parameters
  /// @return The number of parameters
  int param_count() const { return param_count_; }

  /// @brief Check if the statement is still valid
  /// @return True if the statement is valid, false otherwise
  bool is_valid() const { return is_valid_; }

private:
  // The owning connection rebinds this pointer when it moves and nulls it when it
  // disconnects or dies (see PostgreSQLConnection::registered_statements_)
  friend class PostgreSQLConnection;

  PostgreSQLConnection* connection_;
  std::string name_;
  std::string sql_;
  int param_count_;
  bool is_valid_ = true;
};

}  // namespace relx::connection
