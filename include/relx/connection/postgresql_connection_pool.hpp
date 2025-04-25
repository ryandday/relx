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
    /// @brief The connection string for PostgreSQL
    std::string connection_string;
    
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

/// @brief PostgreSQL connection pool that manages a collection of PostgreSQL connections
class PostgreSQLConnectionPool {
public:
    /// @brief Constructor with pool configuration
    /// @param config Configuration for the connection pool
    explicit PostgreSQLConnectionPool(PostgreSQLConnectionPoolConfig config);
    
    /// @brief Destructor that cleans up all connections
    ~PostgreSQLConnectionPool();
    
    // Delete copy and move operations
    PostgreSQLConnectionPool(const PostgreSQLConnectionPool&) = delete;
    PostgreSQLConnectionPool& operator=(const PostgreSQLConnectionPool&) = delete;
    PostgreSQLConnectionPool(PostgreSQLConnectionPool&&) = delete;
    PostgreSQLConnectionPool& operator=(PostgreSQLConnectionPool&&) = delete;
    
    /// @brief Initialize the connection pool
    /// @return Result indicating success or failure
    ConnectionPoolResult<void> initialize();
    
    /// @brief Get a connection from the pool
    /// @return Result containing a connection pointer or an error
    ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> get_connection();
    
    /// @brief Return a connection to the pool
    /// @param connection The connection to return
    void return_connection(std::shared_ptr<PostgreSQLConnection> connection);
    
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
    auto with_connection(Func&& func) 
        -> ConnectionPoolResult<std::invoke_result_t<Func, std::shared_ptr<PostgreSQLConnection>>> {
        
        using ResultType = std::invoke_result_t<Func, std::shared_ptr<PostgreSQLConnection>>;
        
        auto conn_result = get_connection();
        if (!conn_result) {
            // Convert connection pool error to function result error
            if constexpr (std::is_same_v<ResultType, void>) {
                return std::unexpected(conn_result.error());
            } else {
                return std::unexpected(conn_result.error());
            }
        }
        
        auto connection = *conn_result;
        
        try {
            // Execute the function with the connection
            if constexpr (std::is_same_v<ResultType, void>) {
                func(connection);
                return_connection(std::move(connection));
                return {};
            } else {
                auto result = func(connection);
                return_connection(std::move(connection));
                return result;
            }
        } catch (...) {
            // Return the connection to the pool even if an exception occurs
            return_connection(std::move(connection));
            throw; // Re-throw the exception
        }
    }
    
private:
    struct PooledConnection {
        std::shared_ptr<PostgreSQLConnection> connection;
        std::chrono::steady_clock::time_point last_used;
    };
    
    PostgreSQLConnectionPoolConfig config_;
    std::atomic<size_t> active_connections_{0};
    std::atomic<size_t> total_connections_{0};
    
    mutable std::mutex pool_mutex_;
    std::condition_variable conn_available_;
    std::queue<PooledConnection> idle_connections_;
    
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