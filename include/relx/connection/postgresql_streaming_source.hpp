#pragma once

#include "../results/streaming_result.hpp"
#include "postgresql_connection.hpp"
#include "streaming_params.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declarations
struct pg_conn;
using PGconn = pg_conn;

namespace relx::connection {

/// @brief PostgreSQL streaming data source for processing large result sets
/// @details This class implements the data source interface required by StreamingResultSet.
/// It uses PostgreSQL's single-row mode to fetch results incrementally, which is ideal
/// for processing large datasets without loading everything into memory.
class PostgreSQLStreamingSource {
public:
  /// @brief Constructor with connection and query parameters
  /// @param connection PostgreSQL connection to use for queries
  /// @param sql The SQL query string to execute
  /// @param params Optional query parameters
  PostgreSQLStreamingSource(PostgreSQLConnection& connection, std::string sql,
                            std::vector<std::string> params = {});

  /// @brief Constructor with connection and binary query parameters
  /// @param connection PostgreSQL connection to use for queries
  /// @param sql The SQL query string to execute
  /// @param params Query parameters
  /// @param is_binary Vector indicating which parameters are binary
  PostgreSQLStreamingSource(PostgreSQLConnection& connection, std::string sql,
                            std::vector<std::string> params, std::vector<bool> is_binary);

  /// @brief Destructor that cleans up any active query
  ~PostgreSQLStreamingSource();

  // Disable copy operations
  PostgreSQLStreamingSource(const PostgreSQLStreamingSource&) = delete;
  PostgreSQLStreamingSource& operator=(const PostgreSQLStreamingSource&) = delete;

  // Allow move operations
  PostgreSQLStreamingSource(PostgreSQLStreamingSource&& other) noexcept;
  PostgreSQLStreamingSource& operator=(PostgreSQLStreamingSource&& other) noexcept;

  /// @brief Initialize the streaming query
  /// @return ConnectionResult indicating success or failure
  ConnectionResult<void> initialize();

  /// @brief Get the next row from the result set
  /// @return Optional string containing the next row data, or nullopt if no more rows
  /// @details Returns row data in the format "col1|col2|col3|..." for compatibility with LazyRow
  std::optional<std::string> get_next_row();

  /// @brief Get the column names for the result set
  /// @return Vector of column names
  const std::vector<std::string>& get_column_names() const;

  /// @brief Check if the streaming source has been initialized
  /// @return True if initialized, false otherwise
  bool is_initialized() const { return initialized_; }

  /// @brief Check if there are more rows available
  /// @return True if more rows are available, false if end of results or error
  bool has_more_rows() const { return !finished_; }

  /// @brief The error that ended the stream, if any
  /// @details Empty when the stream ended because the rows ran out. Set when the query
  /// failed to start or the server reported an error mid-stream, so that a caller
  /// iterating through StreamingResultSet - which can only observe "no more rows" - can
  /// still tell an empty result apart from a failed one.
  const std::optional<ConnectionError>& last_error() const { return last_error_; }

private:
  // Pointer, not reference: moves must rebind to the moved-from source's connection
  // (a reference member would move-assign *through* the reference instead)
  PostgreSQLConnection* connection_;
  std::string sql_;
  std::vector<std::string> params_;
  std::vector<bool> is_binary_;
  bool use_binary_;

  std::vector<std::string> column_names_;
  std::vector<bool> is_bytea_column_;
  bool initialized_;
  bool finished_;
  bool convert_bytea_;

  // We need to track the active query state
  bool query_active_;

  // Cache for the first row (since we consume it during metadata processing)
  std::optional<std::string> first_row_cached_;

  // Error that ended the stream, if it ended for a reason other than running out of rows
  std::optional<ConnectionError> last_error_;

  /// @brief Helper method to start the streaming query
  ConnectionResult<void> start_query();

  /// @brief Helper method to process column metadata from the first result
  /// @param pg_result PGresult pointer to extract metadata from
  void process_column_metadata(struct pg_result* pg_result);

  /// @brief Helper method to format a single row as a string
  /// @param pg_result PGresult pointer containing the row data
  /// @return Formatted row string or nullopt if no data
  std::optional<std::string> format_row(struct pg_result* pg_result);

  /// @brief Helper method to convert PostgreSQL BYTEA hex format to binary
  /// @param hex_value The hex-encoded BYTEA value from PostgreSQL
  /// @return Binary string representation
  std::string convert_pg_bytea_to_binary(const std::string& hex_value) const;

  /// @brief Helper method to clean up any active query
  void cleanup();

  /// @brief Consume results until libpq reports the command is done
  /// @details libpq only leaves the busy state once PQgetResult returns null, which it
  /// does after the final PGRES_TUPLES_OK. Skipping this leaves the connection unusable
  /// for the next query ("another command is already in progress").
  void drain_results();
};

/// @brief Create a streaming result set from a PostgreSQL connection and query
/// @param connection PostgreSQL connection to use
/// @param sql SQL query to execute
/// @param params Optional query parameters
/// @return StreamingResultSet for processing large result sets incrementally
template <typename... Args>
result::StreamingResultSet<PostgreSQLStreamingSource> create_streaming_result(
    PostgreSQLConnection& connection, const std::string& sql, Args&&... args) {
  // Convert parameters to strings (similar to execute_typed)
  std::vector<std::string> param_strings;
  if constexpr (sizeof...(Args) > 0) {
    param_strings.reserve(sizeof...(Args));

    auto add_param = [&param_strings](auto&& param) {
      using ParamType = std::remove_cvref_t<decltype(param)>;

      if constexpr (std::is_same_v<ParamType, std::nullptr_t>) {
        param_strings.push_back("NULL");
      } else if constexpr (std::is_same_v<ParamType, std::string> ||
                           std::is_same_v<ParamType, const char*> ||
                           std::is_same_v<ParamType, std::string_view>) {
        param_strings.push_back(std::string(param));
      } else if constexpr (std::is_same_v<ParamType, bool>) {
        param_strings.push_back(param ? "t" : "f");
      } else if constexpr (std::is_arithmetic_v<ParamType>) {
        param_strings.push_back(std::to_string(param));
      } else {
        std::ostringstream ss;
        ss << param;
        param_strings.push_back(ss.str());
      }
    };

    (add_param(std::forward<Args>(args)), ...);
  }

  return result::StreamingResultSet<PostgreSQLStreamingSource>(
      PostgreSQLStreamingSource(connection, sql, std::move(param_strings)));
}

/// @brief Create a streaming result set from a PostgreSQL connection and a query object
/// @details Takes the SQL and the bound parameters straight off the query, so a built
/// query streams without the caller restating its parameters:
/// ```cpp
/// auto query = relx::select(u.id, u.name).from(u).where(u.age > 30);
/// auto stream = relx::connection::create_streaming_result(conn, query);
/// ```
/// Parameters travel in their text form (see streaming_text_params).
/// @param connection PostgreSQL connection to use
/// @param query Query object exposing to_sql()/bind_params()
/// @return StreamingResultSet for processing large result sets incrementally
template <query::SqlExpr Query>
result::StreamingResultSet<PostgreSQLStreamingSource> create_streaming_result(
    PostgreSQLConnection& connection, const Query& query) {
  return result::StreamingResultSet<PostgreSQLStreamingSource>(
      PostgreSQLStreamingSource(connection, query.to_sql(), streaming_text_params(query)));
}

}  // namespace relx::connection