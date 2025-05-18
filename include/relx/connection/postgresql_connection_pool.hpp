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
#include <functional>

#include "postgresql_connection.hpp"
#include "relx/connection/connection_pool_base.hpp"

namespace relx {
namespace connection {

/// Configuration for PostgreSQL connection pool
struct PostgreSQLConnectionPoolConfig : ConnectionPoolConfigBase {
    // Add any PostgreSQL-specific configuration options here
};

/// Forward declaration of the pool for use in PooledConnection
class PostgreSQLConnectionPool;

/// A pooled PostgreSQL connection that returns itself to the pool when destroyed
class PostgreSQLPooledConnection {
public:
    PostgreSQLPooledConnection(std::shared_ptr<PostgreSQLConnection> connection, 
                               std::shared_ptr<PostgreSQLConnectionPool> pool)
        : connection_(std::move(connection)), pool_(std::move(pool)) {}
    
    ~PostgreSQLPooledConnection();
    
    PostgreSQLConnection* operator->() const { return connection_.get(); }
    PostgreSQLConnection& operator*() const { return *connection_; }
    
    std::shared_ptr<PostgreSQLConnection> get() const { return connection_; }
    
private:
    std::shared_ptr<PostgreSQLConnection> connection_;
    std::shared_ptr<PostgreSQLConnectionPool> pool_;
};

/// Behavior specialization for synchronous PostgreSQL connections
struct PostgreSQLConnectionBehavior {
    static ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> create_connection(const std::string& connection_string) {
        auto connection = std::make_shared<PostgreSQLConnection>(connection_string);
        
        auto result = connection->connect();
        if (!result) {
            return std::unexpected(ConnectionPoolError{
                .message = "Failed to connect to database: " + result.error().message,
                .error_code = result.error().error_code
            });
        }
        
        return connection;
    }
    
    static bool validate_connection(const std::shared_ptr<PostgreSQLConnection>& connection) {
        // Execute a simple query to validate the connection
        auto result = connection->execute_raw("SELECT 1");
        return result.has_value();
    }
};

/// PostgreSQL connection pool implementation
class PostgreSQLConnectionPool : public ConnectionPoolBase<
    PostgreSQLConnection,
    PostgreSQLPooledConnection,
    PostgreSQLConnectionPoolConfig,
    PostgreSQLConnectionBehavior
> {
public:
    using PooledConnection = PostgreSQLPooledConnection;
    using Base = ConnectionPoolBase<
        PostgreSQLConnection,
        PostgreSQLPooledConnection,
        PostgreSQLConnectionPoolConfig,
        PostgreSQLConnectionBehavior
    >;
    
    /// Create a new connection pool
    /// @param config Configuration for the pool
    /// @return Shared pointer to the new pool
    static std::shared_ptr<PostgreSQLConnectionPool> create(PostgreSQLConnectionPoolConfig config) {
        auto pool = std::make_shared<PostgreSQLConnectionPool>(std::move(config));
        return pool;
    }
    
    explicit PostgreSQLConnectionPool(PostgreSQLConnectionPoolConfig config)
        : Base(std::move(config)) {}
    
    ConnectionPoolResult<PooledConnection> get_connection();
    ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> get_raw_connection();
    
    // Needs to be public to be called by PooledConnection, but should not be called directly
    void return_connection(std::shared_ptr<PostgreSQLConnection> connection) {
        Base::return_connection(std::move(connection));
    }
    
    /// Execute a function with a connection from the pool
    /// @tparam Func Type of the function to execute
    /// @param func Function to execute with a connection
    /// @return Result of the function execution
    template <typename Func>
    auto with_connection(Func&& func) 
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
};

// Implementation of the destructor that returns the connection to the pool
inline PostgreSQLPooledConnection::~PostgreSQLPooledConnection() {
    if (connection_ && pool_) {
        pool_->return_connection(std::move(connection_));
    }
}

} // namespace connection
} // namespace relx 