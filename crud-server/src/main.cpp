#include "db.hpp"
#include "handlers.hpp"
#include "router.hpp"
#include "schema.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = asio::ip::tcp;

std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value ? value : fallback;
}

asio::awaitable<void> session(beast::tcp_stream stream, const crud::Router& router) {
  beast::flat_buffer buffer;
  for (;;) {
    stream.expires_after(std::chrono::seconds(30));

    http::request<http::string_body> req;
    auto [read_ec, read_bytes] =
        co_await http::async_read(stream, buffer, req, asio::as_tuple(asio::use_awaitable));
    if (read_ec) {
      break;  // client closed, timeout, or malformed request
    }

    crud::Response res;
    try {
      res = co_await router.dispatch(req);
    } catch (const std::exception& e) {
      res = crud::error_response(http::status::internal_server_error, e.what());
    }
    res.keep_alive(req.keep_alive());
    res.prepare_payload();

    auto [write_ec, write_bytes] =
        co_await http::async_write(stream, res, asio::as_tuple(asio::use_awaitable));
    if (write_ec || !res.keep_alive()) {
      break;
    }
  }
  beast::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}

asio::awaitable<void> listener(tcp::endpoint endpoint, const crud::Router& router) {
  auto executor = co_await asio::this_coro::executor;
  tcp::acceptor acceptor(executor, endpoint);
  std::cout << "listening on http://" << endpoint << "\n";
  for (;;) {
    auto [ec, socket] = co_await acceptor.async_accept(asio::as_tuple(asio::use_awaitable));
    if (ec) {
      continue;
    }
    asio::co_spawn(executor, session(beast::tcp_stream(std::move(socket)), router),
                   asio::detached);
  }
}

}  // namespace

int main() {
  const auto http_port = static_cast<unsigned short>(std::stoi(env_or("PORT", "8080")));
  const size_t io_threads = 4;

  crud::Db db({
      .connection_params = {.host = env_or("DB_HOST", "localhost"),
                            .port = static_cast<uint16_t>(std::stoi(env_or("DB_PORT", "5434"))),
                            .dbname = env_or("DB_NAME", "relx_test"),
                            .user = env_or("DB_USER", "postgres"),
                            .password = env_or("DB_PASSWORD", "postgres"),
                            .application_name = "relx-crud-server"},
      .initial_size = 4,
      .max_size = 8,
  });

  if (auto initialized = db.initialize(); !initialized) {
    std::cerr << "failed to initialize connection pool: " << initialized.error() << "\n";
    return 1;
  }

  // Schema bootstrap: DDL is derived from the annotated User struct
  {
    asio::io_context bootstrap_io(1);
    auto ddl = asio::co_spawn(
        bootstrap_io,
        db.run([](relx::PostgreSQLConnection& conn) -> crud::DbResult<void> {
          auto created = conn.execute(relx::create_table(crud::users).if_not_exists());
          if (!created) {
            return std::unexpected(crud::DbError{.message = created.error().message});
          }
          return {};
        }),
        asio::use_future);
    bootstrap_io.run();
    if (auto result = ddl.get(); !result) {
      std::cerr << "failed to create table: " << result.error().message << "\n";
      return 1;
    }
  }

  crud::Router router;
  crud::register_user_routes(router, db);

  asio::io_context io(static_cast<int>(io_threads));

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](const boost::system::error_code&, int) { io.stop(); });

  asio::co_spawn(io, listener(tcp::endpoint(tcp::v4(), http_port), router), asio::detached);

  std::vector<std::jthread> workers;
  for (size_t i = 1; i < io_threads; ++i) {
    workers.emplace_back([&io] { io.run(); });
  }
  io.run();
  return 0;
}
