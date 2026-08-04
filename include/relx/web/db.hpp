#pragma once

#include "../connection.hpp"
#include "error.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace relx::web {

/// @brief Map a relx connection error onto an HTTP-shaped ApiError.
/// Unique violations become 409; everything else is a 500.
inline ApiError from_connection_error(const relx::ConnectionError& error) {
  if (error.message.find("duplicate key") != std::string::npos) {
    return conflict("resource already exists");
  }
  return {.status = 500, .message = error.message};
}

/// @brief Awaitable facade over relx's synchronous connection pool.
///
/// Handlers stay coroutines on the HTTP io_context; blocking libpq work is
/// quarantined on a dedicated thread pool sized to match the connection pool
/// (more threads than connections would just queue on the pool anyway). If relx
/// grows a native async pool, only run() changes — handler code keeps the same
/// `co_await db.run(...)` shape.
class Db {
public:
  explicit Db(relx::PostgreSQLConnectionPoolConfig config)
      : threads_(config.max_size),
        pool_(relx::PostgreSQLConnectionPool::create(std::move(config))) {}

  std::expected<void, std::string> initialize() {
    auto result = pool_->initialize();
    if (!result) {
      return std::unexpected(result.error().message);
    }
    return {};
  }

  /// @brief Run `fn(conn)` on a DB thread. `fn` takes relx::PostgreSQLConnection&
  /// and returns ApiResult<T>; pool-acquisition failures become a 503.
  template <typename Fn>
  auto run(Fn fn)
      -> boost::asio::awaitable<std::invoke_result_t<Fn, relx::PostgreSQLConnection&>> {
    using Result = std::invoke_result_t<Fn, relx::PostgreSQLConnection&>;
    co_return co_await boost::asio::co_spawn(
        threads_.get_executor(),
        [this, fn = std::move(fn)]() mutable -> boost::asio::awaitable<Result> {
          auto pooled = pool_->get_connection();
          if (!pooled) {
            co_return std::unexpected(ApiError{
                .status = 503,
                .message = "no database connection available: " + pooled.error().message,
            });
          }
          co_return fn(*pooled->operator->());  // PooledConnection exposes only operator->
        },
        boost::asio::use_awaitable);
  }

private:
  boost::asio::thread_pool threads_;
  std::shared_ptr<relx::PostgreSQLConnectionPool> pool_;
};

}  // namespace relx::web
