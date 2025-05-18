#include "relx/connection/postgresql_connection_pool.hpp"

namespace relx {
namespace connection {

ConnectionPoolResult<PostgreSQLConnectionPool::PooledConnection> 
PostgreSQLConnectionPool::get_connection() {
    auto conn_result = get_raw_connection();
    if (!conn_result) {
        return std::unexpected(conn_result.error());
    }
    
    // First call shared_from_this() to get a shared_ptr to the base class,
    // then cast it to PostgreSQLConnectionPool
    auto self = std::static_pointer_cast<PostgreSQLConnectionPool>(this->shared_from_this());
    
    return PooledConnection(*conn_result, self);
}

ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>> 
PostgreSQLConnectionPool::get_raw_connection() {
    using namespace std::chrono;
    
    // Cleanup old connections first
    this->cleanup_idle_connections();
    
    // Try to get an idle connection or create a new one
    std::unique_lock<std::mutex> lock(this->pool_mutex_);
    
    auto wait_until = steady_clock::now() + this->config_.connection_timeout;
    
    while (this->idle_connections_.empty()) {
        // If we can create a new connection, do so
        if (this->total_connections_ < this->config_.max_size) {
            lock.unlock();
            auto conn_result = PostgreSQLConnectionBehavior::create_connection(this->config_.connection_string);
            lock.lock();
            
            if (!conn_result) {
                return std::unexpected(ConnectionPoolError{
                    .message = "Failed to create new connection: " + conn_result.error().message,
                    .error_code = conn_result.error().error_code
                });
            }
            
            ++this->total_connections_;
            ++this->active_connections_;
            
            return *conn_result;
        }
        
        // Otherwise, wait for a connection to become available
        if (this->conn_available_.wait_until(lock, wait_until) == std::cv_status::timeout) {
            return std::unexpected(ConnectionPoolError{
                .message = "Timed out waiting for a connection",
                .error_code = -1
            });
        }
    }
    
    // Got an idle connection
    auto pooled_connection = std::move(this->idle_connections_.front());
    this->idle_connections_.pop();
    
    auto connection = std::move(pooled_connection.connection);
    
    // Validate the connection if needed
    if (this->config_.validate_connections && !this->validate_connection(connection)) {
        // Connection is invalid, try to create a new one
        lock.unlock();
        auto conn_result = PostgreSQLConnectionBehavior::create_connection(this->config_.connection_string);
        lock.lock();
        
        if (!conn_result) {
            --this->total_connections_; // The invalid connection is effectively gone
            return std::unexpected(ConnectionPoolError{
                .message = "Failed to create replacement connection: " + conn_result.error().message,
                .error_code = conn_result.error().error_code
            });
        }
        
        connection = *conn_result;
    }
    
    ++this->active_connections_;
    return connection;
}

} // namespace connection
} // namespace relx 