#pragma once

#include "postgresql_async_connection.hpp"

#include <atomic>
#include <chrono>
#include <expected>
#include <format>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>

namespace relx::connection {

/// @brief Configuration for PostgreSQL async connection pool
struct PostgreSQLAsyncConnectionPoolConfig {
  /// @brief Connection parameters for PostgreSQL
  PostgreSQLConnectionParams connection_params;

  /// @brief Initial number of connections to create
  size_t initial_size = 5;

  /// @brief Maximum number of connections allowed
  size_t max_size = 10;

  /// @brief Maximum time to wait for a connection before timeout (ms)
  std::chrono::milliseconds connection_timeout{5000};

  /// @brief Whether to validate connections before returning them
  bool validate_connections = true;

  /// @brief Maximum idle time before a connection is closed (ms)
  std::chrono::milliseconds max_idle_time{60000};
};

/// @brief Error type for async connection pool operations
struct AsyncConnectionPoolError {
  std::string message;
  int error_code = 0;
};

/**
 * @brief Format an AsyncConnectionPoolError for exception messages
 */
inline std::string format_error(const AsyncConnectionPoolError& error) {
  return std::format("Async connection pool error: {} (Code: {})", error.message, error.error_code);
}

/// @brief Type alias for result of async connection pool operations
template <typename T>
using AsyncConnectionPoolResult = std::expected<T, AsyncConnectionPoolError>;

// Forward declaration
class PostgreSQLAsyncConnectionPool;

/// @brief PostgreSQL async connection pool that manages a collection of PostgreSQL async
/// connections
class PostgreSQLAsyncConnectionPool
    : public std::enable_shared_from_this<PostgreSQLAsyncConnectionPool> {
private:
  /// @brief Internal structure for tracking idle connections in the pool
  struct PoolEntry {
    std::shared_ptr<PostgreSQLAsyncConnection> connection;
    std::chrono::steady_clock::time_point last_used;
  };

  /// @brief Constructor with pool configuration
  /// @param io_context Boost.Asio IO context for async operations
  /// @param config Configuration for the connection pool
  explicit PostgreSQLAsyncConnectionPool(boost::asio::io_context& io_context,
                                         PostgreSQLAsyncConnectionPoolConfig config);

public:
  // Forward declaration of the connection wrapper class
  class AsyncPooledConnection;

  /// @brief Create a new async connection pool
  /// @note The pool needs to be a shared_ptr to help with pool worker lifetime management,
  ///       so we only allow creation via the create() function.
  /// @param io_context Boost.Asio IO context for async operations
  /// @param config Configuration for the pool
  /// @return Shared pointer to the new pool
  static std::shared_ptr<PostgreSQLAsyncConnectionPool> create(
      boost::asio::io_context& io_context, PostgreSQLAsyncConnectionPoolConfig config) {
    // Use new directly instead of make_shared to access the private constructor
    return std::shared_ptr<PostgreSQLAsyncConnectionPool>(
        new PostgreSQLAsyncConnectionPool(io_context, std::move(config)));
  }

  /// @brief Destructor that cleans up all connections
  ~PostgreSQLAsyncConnectionPool();

  // Delete copy and move operations
  PostgreSQLAsyncConnectionPool(const PostgreSQLAsyncConnectionPool&) = delete;
  PostgreSQLAsyncConnectionPool& operator=(const PostgreSQLAsyncConnectionPool&) = delete;
  PostgreSQLAsyncConnectionPool(PostgreSQLAsyncConnectionPool&&) = delete;
  PostgreSQLAsyncConnectionPool& operator=(PostgreSQLAsyncConnectionPool&&) = delete;

  /// @brief Initialize the connection pool asynchronously
  /// @return Awaitable that resolves to success or failure
  [[nodiscard]] boost::asio::awaitable<AsyncConnectionPoolResult<void>> initialize();

  /// @brief Get a connection from the pool with automatic return when out of scope
  /// @return Awaitable that resolves to AsyncPooledConnection or an error
  [[nodiscard]] boost::asio::awaitable<AsyncConnectionPoolResult<AsyncPooledConnection>>
  get_connection();

  /// @brief Get the current number of active connections
  /// @return The number of active connections
  size_t active_connections() const;

  /// @brief Get the current number of idle connections
  /// @return The number of idle connections
  size_t idle_connections() const;

  /// @brief Execute a function with a connection from the pool
  /// @tparam Func Type of the function to execute
  /// @param func Function to execute with a connection
  /// @return Awaitable that resolves to the result of the function execution
  template <typename Func>
  [[nodiscard]] auto with_connection(Func func) -> boost::asio::awaitable<
      AsyncConnectionPoolResult<std::invoke_result_t<Func, AsyncPooledConnection&>>> {
    using ResultType = std::invoke_result_t<Func, AsyncPooledConnection&>;

    auto conn_result = co_await get_connection();
    if (!conn_result) {
      // Convert connection pool error to function result error
      co_return std::unexpected(conn_result.error());
    }

    try {
      // Execute the function with the connection by reference
      if constexpr (std::is_same_v<ResultType, void>) {
        func(*conn_result);
        co_return AsyncConnectionPoolResult<void>{};
      } else {
        auto result = func(*conn_result);
        co_return result;
      }
    } catch (...) {
      // Connection will be returned automatically by AsyncPooledConnection's destructor
      throw;  // Re-throw the exception
    }
  }

  /// @brief A wrapper for an async connection that automatically returns it to the pool
  class AsyncPooledConnection {
  private:
    std::shared_ptr<PostgreSQLAsyncConnection> connection_;
    std::weak_ptr<PostgreSQLAsyncConnectionPool> pool_;

  public:
    /// @brief Constructor takes a connection and its parent pool
    /// @param connection The database connection
    /// @param pool The connection pool that owns this connection
    AsyncPooledConnection(std::shared_ptr<PostgreSQLAsyncConnection> connection,
                          const std::shared_ptr<PostgreSQLAsyncConnectionPool>& pool)
        : connection_(std::move(connection)), pool_(pool) {}

    /// @brief Destructor automatically returns connection to pool if available
    ~AsyncPooledConnection() {
      if (connection_) {
        // Check if pool still exists
        if (auto pool = pool_.lock()) {
          // Return connection asynchronously in fire-and-forget mode
          boost::asio::co_spawn(
              pool->io_context_,
              [pool, conn = std::move(connection_)]()
                  -> boost::asio::awaitable<void> {  // NOLINT parameters are guaranteed to be alive
                co_await pool->return_connection(conn);
              },
              boost::asio::detached);
        }
        // If pool no longer exists, connection will simply be destroyed
      }
    }

    // Delete copy operations
    AsyncPooledConnection(const AsyncPooledConnection&) = delete;
    AsyncPooledConnection& operator=(const AsyncPooledConnection&) = delete;

    // Allow move operations
    AsyncPooledConnection(AsyncPooledConnection&&) = default;
    AsyncPooledConnection& operator=(AsyncPooledConnection&&) = default;

    /// @brief Forward -> operator to the underlying connection
    PostgreSQLAsyncConnection* operator->() { return connection_.get(); }

    /// @brief Forward const -> operator to the underlying connection
    const PostgreSQLAsyncConnection* operator->() const { return connection_.get(); }

    /// @brief Allow checking if connection is valid
    explicit operator bool() const { return connection_ != nullptr; }
  };

private:
  boost::asio::io_context& io_context_;
  PostgreSQLAsyncConnectionPoolConfig config_;
  std::atomic<size_t> active_connections_{0};
  std::atomic<size_t> total_connections_{0};

  // Use a queue with mutex and condition variable for async coordination
  mutable std::mutex pool_mutex_;
  mutable boost::asio::steady_timer connection_available_signal_;
  std::queue<PoolEntry> idle_connections_;

  // Timer for cleanup operations
  std::unique_ptr<boost::asio::steady_timer> cleanup_timer_;

  /// @brief Get a raw connection from the pool
  /// @return Awaitable that resolves to a connection pointer or an error
  [[nodiscard]] boost::asio::awaitable<
      AsyncConnectionPoolResult<std::shared_ptr<PostgreSQLAsyncConnection>>>
  get_raw_connection();

  /// @brief Return a connection to the pool
  /// @param connection The connection to return
  boost::asio::awaitable<void> return_connection(
      std::shared_ptr<PostgreSQLAsyncConnection> connection);

  /// @brief Create a new connection
  /// @return Awaitable that resolves to a connection pointer or an error
  boost::asio::awaitable<AsyncConnectionPoolResult<std::shared_ptr<PostgreSQLAsyncConnection>>>
  create_connection();

  /// @brief Validate a connection is still usable
  /// @param connection The connection to validate
  /// @return Awaitable that resolves to true if the connection is valid, false otherwise
  boost::asio::awaitable<bool> validate_connection(
      const std::shared_ptr<PostgreSQLAsyncConnection>& connection);

  /// @brief Clean up idle connections that have been idle for too long
  boost::asio::awaitable<void> cleanup_idle_connections();

  /// @brief Start the cleanup timer
  void start_cleanup_timer();
};

}  // namespace relx::connection