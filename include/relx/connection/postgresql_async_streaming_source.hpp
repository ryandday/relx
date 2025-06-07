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

/// @brief Helper trait to detect if a type is boost::asio::awaitable
template <typename T>
struct is_awaitable : std::false_type {};

template <typename T>
struct is_awaitable<boost::asio::awaitable<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_awaitable_v = is_awaitable<T>::value;

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

  /// @brief Get reference to the underlying connection
  /// @return Reference to the PostgreSQL async connection
  PostgreSQLAsyncConnection& get_connection() { return connection_; }

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
    async_streaming_iterator(DataSource& source, AsyncStreamingResultSet& result_set,
                             bool at_end = false)
        : source_(source), result_set_(result_set), current_row_(), at_end_(at_end) {}

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
        // Automatically reset connection state when streaming completes
        co_await result_set_.auto_reset_connection_state();
      }
    }

    bool is_at_end() const { return at_end_; }

  private:
    DataSource& source_;
    AsyncStreamingResultSet& result_set_;
    result::LazyRow current_row_;
    bool at_end_;
  };

  AsyncStreamingResultSet(DataSource source) : source_(std::move(source)), reset_called_(false) {}

  /// @brief Destructor that ensures cleanup when result set goes out of scope
  ~AsyncStreamingResultSet() {
    // Call synchronous cleanup for RAII when going out of scope
    if (!reset_called_) {
      reset_called_ = true;
      // Only call reset for PostgreSQL async streaming sources
      if constexpr (std::is_same_v<DataSource, PostgreSQLAsyncStreamingSource>) {
        source_.get_connection().reset_connection_state_sync();
      }
    }
  }

  // Disable copy operations to ensure single ownership
  AsyncStreamingResultSet(const AsyncStreamingResultSet&) = delete;
  AsyncStreamingResultSet& operator=(const AsyncStreamingResultSet&) = delete;

  // Allow move operations
  AsyncStreamingResultSet(AsyncStreamingResultSet&& other) noexcept
      : source_(std::move(other.source_)), reset_called_(other.reset_called_) {
    other.reset_called_ = true;  // Prevent cleanup in moved-from object
  }

  AsyncStreamingResultSet& operator=(AsyncStreamingResultSet&& other) noexcept {
    if (this != &other) {
      // Cleanup current object before move
      if (!reset_called_) {
        reset_called_ = true;
        if constexpr (std::is_same_v<DataSource, PostgreSQLAsyncStreamingSource>) {
          source_.get_connection().reset_connection_state_sync();
        }
      }

      source_ = std::move(other.source_);
      reset_called_ = other.reset_called_;
      other.reset_called_ = true;  // Prevent cleanup in moved-from object
    }
    return *this;
  }

  /// @brief Begin async iteration
  async_streaming_iterator begin() { return async_streaming_iterator(source_, *this); }

  /// @brief End marker for async iteration
  async_streaming_iterator end() { return async_streaming_iterator(source_, *this, true); }

  /// @brief Process all rows with an async callback
  /// @details This method is the recommended way to iterate over async streaming results.
  ///
  /// **Why for_each is needed instead of range-based for loops:**
  ///
  /// Async streaming cannot use standard C++ range-based for loops because:
  /// 1. Each row fetch is an async operation that must be awaited
  /// 2. Range-based for loops expect synchronous iterators with immediate `operator++()`
  /// 3. The async `advance()` method returns `boost::asio::awaitable<void>`
  ///
  /// **Usage Examples:**
  /// ```cpp
  /// // Basic processing (void return):
  /// co_await result.for_each([](const auto& row) {
  ///     auto id = row.get<int>("id");
  ///     std::cout << "Processing ID: " << id << std::endl;
  /// });
  ///
  /// // Early termination (bool return - true breaks):
  /// co_await result.for_each([](const auto& row) -> bool {
  ///     auto id = row.get<int>("id");
  ///     std::cout << "Processing ID: " << id << std::endl;
  ///     return id > 1000; // Break when ID exceeds 1000
  /// });
  ///
  /// // Async processing:
  /// co_await result.for_each([](const auto& row) -> boost::asio::awaitable<void> {
  ///     auto id = row.get<int>("id");
  ///     co_await some_async_operation(id);
  /// });
  ///
  /// // Async processing with early termination:
  /// co_await result.for_each([](const auto& row) -> boost::asio::awaitable<bool> {
  ///     auto id = row.get<int>("id");
  ///     co_await some_async_operation(id);
  ///     return should_stop_processing(id); // Return true to break
  /// });
  /// ```
  ///
  /// **Manual Iteration (for finer-grained control):**
  ///
  /// If you need more control over the iteration process, you can use manual iteration:
  /// ```cpp
  /// auto it = result.begin();
  /// co_await it.advance(); // Get first row
  ///
  /// while (!it.is_at_end()) {
  ///     const auto& row = *it;
  ///
  ///     // Your custom processing logic here
  ///     auto id = row.get<int>("id");
  ///
  ///     // Custom break conditions
  ///     if (some_complex_condition(row)) {
  ///         break;
  ///     }
  ///
  ///     // Custom error handling
  ///     try {
  ///         co_await it.advance();
  ///     } catch (const std::exception& e) {
  ///         // Handle iteration errors
  ///         break;
  ///     }
  /// }
  /// ```
  ///
  /// @tparam Func Function type - can be sync/async, void/bool return
  /// @param func Callback function to process each row
  ///   - Signature: `void(const LazyRow&)` - Process each row
  ///   - Signature: `bool(const LazyRow&)` - Process each row, return true to break
  ///   - Signature: `boost::asio::awaitable<void>(const LazyRow&)` - Async processing
  ///   - Signature: `boost::asio::awaitable<bool>(const LazyRow&)` - Async processing with early
  ///   termination
  /// @return Awaitable that resolves when all rows are processed or early termination occurs
  template <typename Func>
  boost::asio::awaitable<void> for_each(Func&& func) {
    auto it = begin();

    // Advance to first row
    co_await it.advance();

    while (!it.is_at_end()) {
      // Handle different function signatures
      using ReturnType = std::invoke_result_t<Func, decltype(*it)>;

      if constexpr (is_awaitable_v<ReturnType>) {
        // Asynchronous function - return type is boost::asio::awaitable<T>

        // Extract the value type from the awaitable
        using AwaitableValueType = typename ReturnType::value_type;

        if constexpr (std::is_same_v<AwaitableValueType, bool>) {
          // Async function returning bool - check for early termination
          bool result = co_await func(*it);
          if (result) {
            // Early termination - need to manually reset connection state
            co_await auto_reset_connection_state();
            co_return;
          }
        } else {
          // Async function returning void - just await it
          co_await func(*it);
        }
      } else {
        // Synchronous function
        if constexpr (std::is_same_v<ReturnType, bool>) {
          // Sync function returning bool - check for early termination
          if (func(*it)) {
            // Early termination - need to manually reset connection state
            co_await auto_reset_connection_state();
            co_return;
          }
        } else {
          // Sync function returning void
          func(*it);
        }
      }

      co_await it.advance();
    }

    // Connection state reset is automatically called when iterator reaches end via advance()
  }

  /// @brief Explicitly cleanup the streaming source
  /// @return Awaitable that resolves when cleanup is complete
  boost::asio::awaitable<void> cleanup() {
    co_await source_.async_cleanup();
    co_await auto_reset_connection_state();
  }

  /// @brief Internal method to automatically reset connection state (called only once)
  /// @return Awaitable that resolves when reset is complete
  boost::asio::awaitable<void> auto_reset_connection_state() {
    if (!reset_called_) {
      reset_called_ = true;
      // Only call reset for PostgreSQL async streaming sources
      if constexpr (std::is_same_v<DataSource, PostgreSQLAsyncStreamingSource>) {
        auto reset_result = co_await source_.get_connection().reset_connection_state();
        // Note: We don't propagate reset errors since streaming operation was successful
        (void)reset_result;  // Silence unused variable warning
      }
    }
  }

private:
  DataSource source_;
  bool reset_called_;
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