#include "handlers.hpp"
#include "schema.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_future.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>
#include <relx/web.hpp>

namespace {

std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value ? value : fallback;
}

}  // namespace

int main() {
  namespace web = relx::web;
  namespace asio = boost::asio;

  const auto http_port = static_cast<unsigned short>(std::stoi(env_or("PORT", "8080")));
  const size_t io_threads = 4;

  web::Db db({
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
        db.run([](relx::PostgreSQLConnection& conn) -> web::ApiResult<void> {
          auto created = conn.execute(relx::create_table(crud::users).if_not_exists());
          if (!created) {
            return std::unexpected(web::ApiError{.message = created.error().message});
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

  web::Router router;
  crud::register_user_routes(router, db);

  asio::io_context io(static_cast<int>(io_threads));

  asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](const boost::system::error_code&, int) { io.stop(); });

  web::spawn_listener(io, http_port, router);
  std::cout << "listening on http://0.0.0.0:" << http_port << "\n";

  std::vector<std::jthread> workers;
  for (size_t i = 1; i < io_threads; ++i) {
    workers.emplace_back([&io] { io.run(); });
  }
  io.run();
  return 0;
}
