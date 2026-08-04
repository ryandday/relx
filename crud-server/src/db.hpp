#pragma once

#include <expected>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <relx/connection.hpp>

namespace crud {

namespace asio = boost::asio;

/// Error surfaced to handlers, already shaped for an HTTP response
struct DbError {
  int http_status = 500;
  std::string message;
};

template <typename T>
using DbResult = std::expected<T, DbError>;

/// Awaitable facade over relx's synchronous connection pool.
///
/// Handlers stay coroutines on the HTTP io_context; the blocking libpq work is
/// quarantined on a dedicated thread pool sized to match the connection pool
/// (more threads than connections would just queue on the pool anyway). If relx
/// grows a native async pool, only run() changes — handler code keeps the same
/// `co_await db.run(...)` shape.
class Db {
public:
  Db(relx::PostgreSQLConnectionPoolConfig config)
      : threads_(config.max_size),
        pool_(relx::PostgreSQLConnectionPool::create(std::move(config))) {}

  std::expected<void, std::string> initialize() {
    auto result = pool_->initialize();
    if (!result) {
      return std::unexpected(result.error().message);
    }
    return {};
  }

  /// Run `fn(conn)` on a DB thread. `fn` takes relx::PostgreSQLConnection& and
  /// returns DbResult<T>; pool-acquisition failures become DbError{503}.
  template <typename Fn>
  auto run(Fn fn) -> asio::awaitable<std::invoke_result_t<Fn, relx::PostgreSQLConnection&>> {
    using Result = std::invoke_result_t<Fn, relx::PostgreSQLConnection&>;
    co_return co_await asio::co_spawn(
        threads_.get_executor(),
        [this, fn = std::move(fn)]() mutable -> asio::awaitable<Result> {
          auto pooled = pool_->get_connection();
          if (!pooled) {
            co_return std::unexpected(DbError{
                .http_status = 503,
                .message = "no database connection available: " + pooled.error().message,
            });
          }
          co_return fn(*pooled->operator->());  // PooledConnection exposes only operator->
        },
        asio::use_awaitable);
  }

private:
  asio::thread_pool threads_;
  std::shared_ptr<relx::PostgreSQLConnectionPool> pool_;
};

}  // namespace crud
