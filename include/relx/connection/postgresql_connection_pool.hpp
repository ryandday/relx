#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <optional>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "postgresql_connection.hpp"

namespace relx {
namespace connection {

/// @brief Configuration for PostgreSQL connection pool
struct PostgreSQLConnectionPoolConfig {
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

/// @brief Error type for connection pool operations
struct ConnectionPoolError {
    std::string message;
    int error_code = 0;
};

/// @brief Type alias for result of connection pool operations
template <typename T>
using ConnectionPoolResult = std::expected<T, ConnectionPoolError>;

// Forward declaration
class PostgreSQLConnectionPool;

/// @brief PostgreSQL connection pool that manages a collection of PostgreSQL connections
class PostgreSQLConnectionPool : public std::enable_shared_from_this<PostgreSQLConnectionPool> {
private:
    /// @brief Internal structure for tracking idle connections in the pool
    struct PoolEntry {
        std::shared_ptr<PostgreSQLConnection> connection;
        std::chrono::steady_clock::time_point last_used;
    };

    /// @brief Constructor with pool configuration
    /// @param config Configuration for the connection pool
    explicit PostgreSQLConnectionPool(PostgreSQLConnectionPoolConfig config);

public:
    // Forward declaration of the connection wrapper class
    class PooledConnection;

    /// @brief Create a new connection pool
    /// @note The pool needs to be a shared_ptr to help with pool worker lifetime management, 
    ///       so we only allow creation via the create() function.



    /// @param config Configuration for the pool
    /// @return Shared pointer to the new pool
    static std::shared_ptr<PostgreSQLConnectionPool> create(PostgreSQLConnectionPoolConfig config) {
        // Use new directly instead of make_shared to access the private constructor
        return std::shared_ptr<PostgreSQLConnectionPool>(new PostgreSQLConnectionPool(std::move(config)));
    }

    /// @brief Destructor that cleans up all connections
    ~PostgreSQLConnectionPool();

    // Delete copy and move operations
    PostgreSQLConnectionPool(const PostgreSQLConnectionPool&) = delete;
    PostgreSQLConnectionPool& operator=(const PostgreSQLConnectionPool&) = delete;
    PostgreSQLConnectionPool(PostgreSQLConnectionPool&&) = delete;
    PostgreSQLConnectionPool& operator=(PostgreSQLConnectionPool&&) = delete;
    
    /// @brief Initialize the connection pool
    /// @return Result indicating success or failure
    [[nodiscard]] ConnectionPoolResult<void> initialize();
    
    /// @brief Get a connection from the pool with automatic return when out of scope
    /// @return Result containing a PooledConnection or an error
    [[nodiscard]] ConnectionPoolResult<PooledConnection> get_connection();


    /// @brief Get the current number of active connections
    /// @return The number of active connections
    size_t active_connections() const;
    
    /// @brief Get the current number of idle connections
    /// @return The number of idle connections
    size_t idle_connections() const;

    /// @brief Execute a function with a connection from the pool
    /// @tparam Func Type of the function to execute
    /// @param func Function to execute with a connection
    /// @return Result of the function execution
    template <typename Func>
    [[nodiscard]] auto with_connection(Func&& func) 
        -> ConnectionPoolResult<std::invoke_result_t<Func, PooledConnection&>> {

        using ResultType = std::invoke_result_t<Func, PooledConnection&>;

        auto conn_result = get_connection();
        if (!conn_result) {
            // Convert connection pool error to function result error
            return std::unexpected(conn_result.error());
        }

        try {
            // Execute the function with the connection by reference
            if constexpr (std::is_same_v<ResultType, void>) {
                func(*conn_result);
                return {};
            } else {
                return func(*conn_result);
            }
        } catch (...) {
            // Connection will be returned automatically by PooledConnection's destructor
            throw; // Re-throw the exception
        }
    }
    
    /// @brief A wrapper for a connection that automatically returns it to the pool
    class PooledConnection {
    private:
        std::shared_ptr<PostgreSQLConnection> connection_;
        std::weak_ptr<PostgreSQLConnectionPool> pool_;
    
    public:
        /// @brief Constructor takes a connection and its parent pool
        /// @param connection The database connection
        /// @param pool The connection pool that owns this connection
        PooledConnection(std::shared_ptr<PostgreSQLConnection> connection, 
                       std::shared_ptr<PostgreSQLConnectionPool> pool)
            : connection_(std::move(connection)), pool_(pool) {}
        
        /// @brief Destructor automatically returns connection to pool if available
        ~PooledConnection() {
            if (connection_) {
                // Check if pool still exists
                if (auto pool = pool_.lock()) {
                    pool->return_connection(std::move(connection_));
                }
                // If pool no longer exists, connection will simply be destroyed
            }
        }
        
        // Delete copy operations
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;
        
        // Allow move operations
        PooledConnection(PooledConnection&&) = default;
        PooledConnection& operator=(PooledConnection&&) = default;
        
        /// @brief Forward -> operator to the underlying connection
        PostgreSQLConnection* operator->() { return connection_.get(); }
        
        /// @brief Forward const -> operator to the underlying connection
        const PostgreSQLConnection* operator->() const { return connection_.get(); }
        
        /// @brief Allow checking if connection is valid
        explicit operator bool() const { return connection_ != nullptr; }
    };
    
private:
    PostgreSQLConnectionPoolConfig config_;
    std::atomic<size_t> active_connections_{0};
    std::atomic<size_t> total_connections_{0};
    
    mutable std::mutex pool_mutex_;
    std::condition_variable conn_available_;
    std::queue<PoolEntry> idle_connections_;
    
    /// @brief Get a raw connection from the pool 
    /// @return Result containing a connection pointer or an error
    [[nodiscard]] ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> get_raw_connection();
    
    /// @brief Return a connection to the pool
    /// @param connection The connection to return
    void return_connection(std::shared_ptr<PostgreSQLConnection> connection);
    
    /// @brief Create a new connection
    /// @return Result containing a connection pointer or an error
    ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> create_connection();
    
    /// @brief Validate a connection is still usable
    /// @param connection The connection to validate
    /// @return True if the connection is valid, false otherwise
    bool validate_connection(const std::shared_ptr<PostgreSQLConnection>& connection) const;
    
    /// @brief Clean up idle connections that have been idle for too long
    void cleanup_idle_connections();
};

} // namespace connection
} // namespace relx 