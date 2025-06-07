#pragma once

#include "../results/streaming_result.hpp"
#include "postgresql_async_connection.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>

// Forward declarations
struct pg_conn;
using PGconn = pg_conn;

namespace relx::connection {

/// @brief Async PostgreSQL streaming data source for processing large result sets
/// @details This class implements an async-compatible data source interface.
/// It uses PostgreSQL's single-row mode with async operations to fetch results incrementally.
class PostgreSQLAsyncStreamingSource {
public:
  /// @brief Constructor with async connection and query parameters
  /// @param connection Async PostgreSQL connection to use for queries
  /// @param sql The SQL query string to execute
  /// @param params Optional query parameters
  PostgreSQLAsyncStreamingSource(PostgreSQLAsyncConnection& connection, std::string sql,
                                 std::vector<std::string> params = {});

  /// @brief Destructor that cleans up any active query
  ~PostgreSQLAsyncStreamingSource();

  // Disable copy operations
  PostgreSQLAsyncStreamingSource(const PostgreSQLAsyncStreamingSource&) = delete;
  PostgreSQLAsyncStreamingSource& operator=(const PostgreSQLAsyncStreamingSource&) = delete;

  // Allow move operations
  PostgreSQLAsyncStreamingSource(PostgreSQLAsyncStreamingSource&& other) noexcept;
  PostgreSQLAsyncStreamingSource& operator=(PostgreSQLAsyncStreamingSource&& other) noexcept;

  /// @brief Initialize the streaming query asynchronously
  /// @return Awaitable that resolves to success or failure
  boost::asio::awaitable<ConnectionResult<void>> initialize();

  /// @brief Get the next row from the result set asynchronously
  /// @return Awaitable that resolves to optional string containing the next row data
  /// @details Returns row data in the format "col1|col2|col3|..." for compatibility with LazyRow
  boost::asio::awaitable<std::optional<std::string>> get_next_row();

  /// @brief Get the column names for the result set
  /// @return Vector of column names
  const std::vector<std::string>& get_column_names() const;

  /// @brief Check if the streaming source has been initialized
  /// @return True if initialized, false otherwise
  bool is_initialized() const { return initialized_; }

  /// @brief Check if there are more rows available
  /// @return True if more rows are available, false if end of results or error
  bool has_more_rows() const { return !finished_; }

  /// @brief Explicitly cleanup any active query asynchronously
  /// @return Awaitable that resolves when cleanup is complete
  boost::asio::awaitable<void> async_cleanup();

private:
  PostgreSQLAsyncConnection& connection_;
  std::string sql_;
  std::vector<std::string> params_;

  std::vector<std::string> column_names_;
  std::vector<bool> is_bytea_column_;
  bool initialized_;
  bool finished_;
  bool convert_bytea_;

  // Track query state
  bool query_active_;

  // Cache for the first row (since we consume it during metadata processing)
  std::optional<std::string> first_row_cached_;

  // Streaming state for proper result management
  std::unique_ptr<struct pg_result, void (*)(struct pg_result*)> current_result_;
  size_t current_row_index_;
  bool has_pending_results_;

  /// @brief Helper method to start the streaming query asynchronously
  boost::asio::awaitable<ConnectionResult<void>> start_query();

  /// @brief Helper method to process column metadata from a PGresult
  void process_column_metadata_from_pg_result(struct pg_result* pg_result);

  /// @brief Helper method to format a single row from a PGresult as a string
  /// @param pg_result The PGresult containing a single row
  /// @return Formatted row string
  std::string format_single_row(struct pg_result* pg_result);

  /// @brief Helper method to convert PostgreSQL BYTEA hex format to binary
  /// @param hex_value The hex-encoded BYTEA value from PostgreSQL
  /// @return Binary string representation
  std::string convert_pg_bytea_to_binary(const std::string& hex_value) const;

  /// @brief Helper method to clean up any active query
  void cleanup();
};

/// @brief Async streaming result set that yields rows asynchronously
template <typename DataSource>
class AsyncStreamingResultSet {
public:
  /// @brief Iterator that reads data on demand asynchronously
  class async_streaming_iterator {
  public:
    async_streaming_iterator(DataSource& source, bool at_end = false)
        : source_(source), current_row_(), at_end_(at_end) {}

    /// @brief Get the current row (must be called after advance())
    const auto& operator*() const { return current_row_; }

    /// @brief Advance to the next row asynchronously
    boost::asio::awaitable<void> advance() {
      if (at_end_) {
        co_return;
      }

      auto next_row_data = co_await source_.get_next_row();
      if (next_row_data) {
        current_row_ = result::LazyRow(std::move(*next_row_data), source_.get_column_names());
      } else {
        at_end_ = true;
      }
    }

    bool is_at_end() const { return at_end_; }

  private:
    DataSource& source_;
    result::LazyRow current_row_;
    bool at_end_;
  };

  AsyncStreamingResultSet(DataSource source) : source_(std::move(source)) {}

  /// @brief Begin async iteration
  async_streaming_iterator begin() { return async_streaming_iterator(source_); }

  /// @brief End marker for async iteration
  async_streaming_iterator end() { return async_streaming_iterator(source_, true); }

  /// @brief Process all rows with an async callback
  template <typename Func>
  boost::asio::awaitable<void> for_each(Func&& func) {
    auto it = begin();

    // Advance to first row
    co_await it.advance();

    while (!it.is_at_end()) {
      if constexpr (std::is_invocable_v<Func, decltype(*it)>) {
        func(*it);
      } else {
        co_await func(*it);
      }
      co_await it.advance();
    }
  }

  /// @brief Explicitly cleanup the streaming source
  /// @return Awaitable that resolves when cleanup is complete
  boost::asio::awaitable<void> cleanup() {
    co_await source_.async_cleanup();
  }

private:
  DataSource source_;
};

/// @brief Create an async streaming result set from a PostgreSQL async connection and query
/// @param connection Async PostgreSQL connection to use
/// @param sql SQL query to execute
/// @param params Optional query parameters
/// @return AsyncStreamingResultSet for processing large result sets incrementally
template <typename... Args>
AsyncStreamingResultSet<PostgreSQLAsyncStreamingSource> create_async_streaming_result(
    PostgreSQLAsyncConnection& connection, const std::string& sql, Args&&... args) {
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
      } else if constexpr (std::is_arithmetic_v<ParamType>) {
        param_strings.push_back(std::to_string(param));
      } else if constexpr (std::is_same_v<ParamType, bool>) {
        param_strings.push_back(param ? "t" : "f");
      } else {
        std::ostringstream ss;
        ss << param;
        param_strings.push_back(ss.str());
      }
    };

    (add_param(std::forward<Args>(args)), ...);
  }

  return AsyncStreamingResultSet<PostgreSQLAsyncStreamingSource>(
      PostgreSQLAsyncStreamingSource(connection, sql, std::move(param_strings)));
}

}  // namespace relx::connection