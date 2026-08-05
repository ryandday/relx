#include "relx/connection/postgresql_connection_pool.hpp"

namespace relx::connection {

namespace {

/// Decrement an unsigned counter without wrapping below zero
void decrement_guarded(std::atomic<size_t>& counter) {
  size_t current = counter.load();
  while (current > 0 && !counter.compare_exchange_weak(current, current - 1)) {
  }
}

void subtract_guarded(std::atomic<size_t>& counter, size_t amount) {
  size_t current = counter.load();
  while (!counter.compare_exchange_weak(current, current >= amount ? current - amount : 0)) {
  }
}

}  // namespace

PostgreSQLConnectionPool::PostgreSQLConnectionPool(PostgreSQLConnectionPoolConfig config)
    : config_(std::move(config)) {}

PostgreSQLConnectionPool::~PostgreSQLConnectionPool() {
  // Clear all idle connections
  const std::lock_guard<std::mutex> lock(pool_mutex_);
  while (!idle_connections_.empty()) {
    idle_connections_.pop();
  }
}

ConnectionPoolResult<void> PostgreSQLConnectionPool::initialize() {
  // Reserve the slots up front (so concurrent callers cannot overshoot), then
  // connect WITHOUT holding the pool mutex: N blocking PQconnectdb calls under
  // the lock would block every other pool operation for the whole connect window.
  size_t to_create = 0;
  {
    const std::lock_guard<std::mutex> lock(pool_mutex_);
    const size_t existing = total_connections_.load();
    if (existing >= config_.initial_size) {
      return {};  // already initialized; calling again is a no-op
    }
    to_create = config_.initial_size - existing;
    total_connections_ += to_create;
  }

  std::vector<PoolEntry> created;
  created.reserve(to_create);
  for (size_t i = 0; i < to_create; ++i) {
    auto conn_result = create_connection();
    if (!conn_result) {
      // Keep what connected so far, release the unused reserved slots
      subtract_guarded(total_connections_, to_create - created.size());
      const std::lock_guard<std::mutex> lock(pool_mutex_);
      for (auto& entry : created) {
        idle_connections_.push(std::move(entry));
        conn_available_.notify_one();
      }
      return std::unexpected(ConnectionPoolError{
          .message = "Failed to initialize connection pool: " + conn_result.error().message,
          .error_code = conn_result.error().error_code});
    }
    created.push_back({.connection = *conn_result, .last_used = std::chrono::steady_clock::now()});
  }

  const std::lock_guard<std::mutex> lock(pool_mutex_);
  for (auto& entry : created) {
    idle_connections_.push(std::move(entry));
    conn_available_.notify_one();
  }
  return {};
}

ConnectionPoolResult<PostgreSQLConnectionPool::PooledConnection>
PostgreSQLConnectionPool::get_connection() {
  auto conn_result = get_raw_connection();
  if (!conn_result) {
    return std::unexpected(conn_result.error());
  }

  return PooledConnection(*conn_result, shared_from_this());
}

ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>>
PostgreSQLConnectionPool::get_raw_connection() {
  using namespace std::chrono;

  // Cleanup old connections first
  cleanup_idle_connections();

  std::unique_lock<std::mutex> lock(pool_mutex_);

  auto wait_until = steady_clock::now() + config_.connection_timeout;

  while (true) {
    // Prefer an idle connection; validation runs with the mutex released so a
    // stale socket cannot stall every other checkout
    while (!idle_connections_.empty()) {
      auto pooled_connection = std::move(idle_connections_.front());
      idle_connections_.pop();
      auto connection = std::move(pooled_connection.connection);

      if (config_.validate_connections) {
        lock.unlock();
        if (!validate_connection(connection)) {
          // Discard the dead connection; its slot frees capacity for a waiter
          decrement_guarded(total_connections_);
          conn_available_.notify_one();
          lock.lock();
          continue;
        }
        ++active_connections_;
        return connection;
      }

      ++active_connections_;
      return connection;
    }

    // No idle connection: create one if capacity allows. The slot is reserved
    // before the mutex is released, so concurrent creators cannot overshoot
    // max_size (check and increment are serialized by the mutex).
    if (total_connections_.load() < config_.max_size) {
      ++total_connections_;
      lock.unlock();

      auto conn_result = create_connection();
      if (!conn_result) {
        decrement_guarded(total_connections_);
        conn_available_.notify_one();
        return std::unexpected(ConnectionPoolError{.message = "Failed to create new connection: " +
                                                              conn_result.error().message,
                                                   .error_code = conn_result.error().error_code});
      }

      ++active_connections_;
      return *conn_result;
    }

    // Otherwise, wait for a connection to become available
    if (conn_available_.wait_until(lock, wait_until) == std::cv_status::timeout) {
      return std::unexpected(
          ConnectionPoolError{.message = "Timed out waiting for a connection", .error_code = -1});
    }
  }
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

  const std::lock_guard<std::mutex> lock(pool_mutex_);

  decrement_guarded(active_connections_);

  if (!is_valid) {
    // Discard invalid connection; the freed slot lets a waiter create anew
    decrement_guarded(total_connections_);
    conn_available_.notify_one();
  } else {
    // Return to the pool
    idle_connections_.push(
        {.connection = std::move(connection), .last_used = std::chrono::steady_clock::now()});
    conn_available_.notify_one();
  }
}

size_t PostgreSQLConnectionPool::active_connections() const {
  return active_connections_.load();
}

size_t PostgreSQLConnectionPool::idle_connections() const {
  const std::lock_guard<std::mutex> lock(pool_mutex_);
  return idle_connections_.size();
}

ConnectionPoolResult<std::shared_ptr<PostgreSQLConnection>>
PostgreSQLConnectionPool::create_connection() {
  auto connection = std::make_shared<PostgreSQLConnection>(config_.connection_params);

  auto result = connection->connect();
  if (!result) {
    return std::unexpected(
        ConnectionPoolError{.message = "Failed to connect to database: " + result.error().message,
                            .error_code = result.error().error_code});
  }

  return connection;
}

bool PostgreSQLConnectionPool::validate_connection(
    const std::shared_ptr<PostgreSQLConnection>& connection) {
  if (!connection->is_connected()) {
    return false;
  }

  // Execute a simple query to validate the connection
  auto result = connection->execute_raw("SELECT 1");
  return result.has_value();
}

void PostgreSQLConnectionPool::cleanup_idle_connections() {
  using namespace std::chrono;

  const std::lock_guard<std::mutex> lock(pool_mutex_);

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

    const bool should_close = idle_time > config_.max_idle_time &&
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
  subtract_guarded(total_connections_, closed);
}

}  // namespace relx::connection
