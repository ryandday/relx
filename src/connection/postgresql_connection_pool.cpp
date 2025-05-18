#include "relx/connection/postgresql_connection_pool.hpp"

namespace relx {
namespace connection {

PostgreSQLConnectionPool::PostgreSQLConnectionPool(PostgreSQLConnectionPoolConfig config)
    : config_(std::move(config)) {}

PostgreSQLConnectionPool::~PostgreSQLConnectionPool() {
    // Clear all idle connections
    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
}

ConnectionPoolResult<void> PostgreSQLConnectionPool::initialize() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Create initial connections
    for (size_t i = 0; i < config_.initial_size; ++i) {
        auto conn_result = create_connection();
        if (!conn_result) {
            // If we can't create the initial connections, consider it a failure
            return std::unexpected(ConnectionPoolError{
                .message = "Failed to initialize connection pool: " + conn_result.error().message,
                .error_code = conn_result.error().error_code
            });
        }
        
        idle_connections_.push({
            .connection = *conn_result,
            .last_used = std::chrono::steady_clock::now()
        });
    }
    
    total_connections_ = config_.initial_size;
    return {};
}

ConnectionPoolResult<PostgreSQLConnectionPool::PooledConnection> PostgreSQLConnectionPool::get_connection() {
    auto conn_result = get_raw_connection();
    if (!conn_result) {
        return std::unexpected(conn_result.error());
    }

    return PooledConnection(*conn_result, shared_from_this());




}

ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> PostgreSQLConnectionPool::get_raw_connection() {

    using namespace std::chrono;

    // Cleanup old connections first
    cleanup_idle_connections();

    // Try to get an idle connection or create a new one
    std::unique_lock<std::mutex> lock(pool_mutex_);

    auto wait_until = steady_clock::now() + config_.connection_timeout;

    while (idle_connections_.empty()) {
        // If we can create a new connection, do so
        if (total_connections_ < config_.max_size) {
            lock.unlock();
            auto conn_result = create_connection();
            lock.lock();

            if (!conn_result) {
                return std::unexpected(ConnectionPoolError{
                    .message = "Failed to create new connection: " + conn_result.error().message,
                    .error_code = conn_result.error().error_code
                });
            }

            ++total_connections_;
            ++active_connections_;

            return *conn_result;
        }

        // Otherwise, wait for a connection to become available
        if (conn_available_.wait_until(lock, wait_until) == std::cv_status::timeout) {
            return std::unexpected(ConnectionPoolError{
                .message = "Timed out waiting for a connection",
                .error_code = -1
            });
        }
    }

    // Got an idle connection
    auto pooled_connection = std::move(idle_connections_.front());
    idle_connections_.pop();

    auto connection = std::move(pooled_connection.connection);

    // Validate the connection if needed
    if (config_.validate_connections && !validate_connection(connection)) {
        // Connection is invalid, try to create a new one
        lock.unlock();
        auto conn_result = create_connection();
        lock.lock();

        if (!conn_result) {
            --total_connections_; // The invalid connection is effectively gone
            return std::unexpected(ConnectionPoolError{
                .message = "Failed to create replacement connection: " + conn_result.error().message,
                .error_code = conn_result.error().error_code
            });
        }

        connection = *conn_result;
    }

    ++active_connections_;
    return connection;
}

void PostgreSQLConnectionPool::return_connection(std::shared_ptr<PostgreSQLConnection> connection) {
    if (!connection) {
        return;
    }
    
    // Check if the connection is still valid
    bool is_valid = connection->is_connected();
    
    if (is_valid) {
        // If there's an active transaction, roll it back
        if (connection->in_transaction()) {
            auto result = connection->rollback_transaction();
            is_valid = result.has_value();
        }
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    --active_connections_;
    
    if (!is_valid) {
        // Discard invalid connection
        --total_connections_;
    } else {
        // Return to the pool
        idle_connections_.push({
            .connection = std::move(connection),
            .last_used = std::chrono::steady_clock::now()
        });
        conn_available_.notify_one();
    }
}

size_t PostgreSQLConnectionPool::active_connections() const {
    return active_connections_.load();
}

size_t PostgreSQLConnectionPool::idle_connections() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    return idle_connections_.size();
}

ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> PostgreSQLConnectionPool::create_connection() {
    auto connection = std::make_shared<PostgreSQLConnection>(config_.connection_string);
    
    auto result = connection->connect();
    if (!result) {
        return std::unexpected(ConnectionPoolError{
            .message = "Failed to connect to database: " + result.error().message,
            .error_code = result.error().error_code
        });
    }
    
    return connection;
}

bool PostgreSQLConnectionPool::validate_connection(const std::shared_ptr<PostgreSQLConnection>& connection) const {
    if (!connection->is_connected()) {
        return false;
    }
    
    // Execute a simple query to validate the connection
    auto result = connection->execute_raw("SELECT 1");
    return result.has_value();
}

void PostgreSQLConnectionPool::cleanup_idle_connections() {
    using namespace std::chrono;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (idle_connections_.empty()) {
        return;
    }
    
    // Keep at least config_.initial_size connections
    if (total_connections_ <= config_.initial_size) {
        return;
    }
    
    // Get the current time
    auto now = steady_clock::now();
    
    // Create a temporary queue to hold connections we want to keep
    std::queue<PoolEntry> keep_connections;
    
    size_t closed = 0;
    
    // Process all idle connections
    while (!idle_connections_.empty()) {
        auto& pooled_conn = idle_connections_.front();
        
        // Check if the connection has been idle for too long
        auto idle_time = now - pooled_conn.last_used;
        
        bool should_close = idle_time > config_.max_idle_time && 
                          (total_connections_ - closed) > config_.initial_size;
        
        if (should_close) {
            // Close this connection
            ++closed;
        } else {
            // Keep this connection
            keep_connections.push(std::move(pooled_conn));
        }
        
        idle_connections_.pop();
    }
    
    // Update the idle connections queue
    idle_connections_ = std::move(keep_connections);
    
    // Update the total connection count
    total_connections_ -= closed;
}

} // namespace connection
} // namespace relx 