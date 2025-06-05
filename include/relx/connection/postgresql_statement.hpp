#pragma once

#include "postgresql_connection.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Forward declaration
struct pg_result;
using PGresult = pg_result;
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
  /// @param params Vector of parameter values
  /// @return Result containing the query results or an error
  ConnectionResult<result::ResultSet> execute(const std::vector<std::string>& params = {});

  /// @brief Execute the prepared statement with typed parameters
  /// @tparam Args The types of the parameters
  /// @param args The parameter values
  /// @return Result containing the query results or an error
  template <typename... Args>
  ConnectionResult<result::ResultSet> execute_typed(Args&&... args) {
    // Convert each parameter to its string representation
    std::vector<std::string> param_strings;
    param_strings.reserve(sizeof...(Args));

    // Helper to convert a parameter to string and add to vector
    auto add_param = [&param_strings](auto&& param) {
      using ParamType = std::remove_cvref_t<decltype(param)>;

      if constexpr (std::is_same_v<ParamType, std::nullptr_t>) {
        // Handle NULL values
        param_strings.push_back("NULL");
      } else if constexpr (std::is_same_v<ParamType, std::string> ||
                           std::is_same_v<ParamType, const char*> ||
                           std::is_same_v<ParamType, std::string_view>) {
        // String types
        param_strings.push_back(std::string(param));
      } else if constexpr (std::is_arithmetic_v<ParamType>) {
        // Numeric types
        param_strings.push_back(std::to_string(param));
      } else if constexpr (std::is_same_v<ParamType, bool>) {
        // Boolean values
        param_strings.push_back(param ? "t" : "f");
      } else {
        // Other types, try to use stream conversion
        std::ostringstream ss;
        ss << param;
        param_strings.push_back(ss.str());
      }
    };

    // Add each parameter to the vector
    (add_param(std::forward<Args>(args)), ...);

    // Call the string-based execute with our converted parameters
    return execute(param_strings);
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
  PostgreSQLConnection& connection_;
  std::string name_;
  std::string sql_;
  int param_count_;
  bool is_valid_ = true;

  /// @brief Execute the prepared statement with parameters
  /// @param params Vector of parameter values
  /// @param result_format Format of the result (0 for text, 1 for binary)
  /// @return PGresult pointer or error
  ConnectionResult<PGresult*> execute_raw(const std::vector<std::string>& params,
                                          int result_format = 0);

  /// @brief Process PGresult into a ResultSet
  /// @param pg_result The PGresult to process
  /// @return ResultSet or error
  ConnectionResult<result::ResultSet> process_result(PGresult* pg_result);

  /// @brief Helper to escape string literals for PostgreSQL
  /// @param str The string to escape
  /// @return The escaped string
  static std::string escape_string(const std::string& str);
};

}  // namespace relx::connection