#pragma once

#include "db.hpp"
#include "router.hpp"
#include "schema.hpp"

#include <charconv>
#include <optional>
#include <string_view>

#include <glaze/glaze.hpp>
#include <relx/query.hpp>

namespace crud {

inline std::optional<int> parse_int(std::string_view s) {
  int value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

/// Map a relx connection error onto an HTTP-shaped DbError
inline DbError to_db_error(const relx::ConnectionError& error) {
  if (error.message.find("duplicate key") != std::string::npos) {
    return {.http_status = 409, .message = "email already exists"};
  }
  return {.http_status = 500, .message = error.message};
}

template <typename T>
Response json_response(http::status status, const T& value) {
  auto json = glz::write_json(value);
  if (!json) {
    return error_response(http::status::internal_server_error, "serialization failed");
  }
  return make_response(status, std::move(*json));
}

inline Response from_db_error(const DbError& error) {
  return error_response(static_cast<http::status>(error.http_status), error.message);
}

inline auto select_user_columns() {
  return relx::query::select(users.id, users.name, users.email, users.bio).from(users);
}

inline DbResult<User> fetch_user(relx::PostgreSQLConnection& conn, int id) {
  auto rows = conn.execute_many<User>(select_user_columns().where(users.id == id));
  if (!rows) {
    return std::unexpected(to_db_error(rows.error()));
  }
  if (rows->empty()) {
    return std::unexpected(DbError{.http_status = 404, .message = "user not found"});
  }
  return rows->front();
}

inline void register_user_routes(Router& router, Db& db) {
  router.add(http::verb::get, "/users", [&db](const RequestContext&) -> asio::awaitable<Response> {
    auto result = co_await db.run([](relx::PostgreSQLConnection& conn) -> DbResult<std::vector<User>> {
      auto rows = conn.execute_many<User>(select_user_columns().order_by(users.id));
      if (!rows) {
        return std::unexpected(to_db_error(rows.error()));
      }
      return std::move(*rows);
    });
    co_return result ? json_response(http::status::ok, *result) : from_db_error(result.error());
  });

  router.add(http::verb::get, "/users/:id",
             [&db](const RequestContext& ctx) -> asio::awaitable<Response> {
               auto id = parse_int(ctx.param("id"));
               if (!id) {
                 co_return error_response(http::status::bad_request, "id must be an integer");
               }
               auto result = co_await db.run([id = *id](relx::PostgreSQLConnection& conn) {
                 return fetch_user(conn, id);
               });
               co_return result ? json_response(http::status::ok, *result)
                                : from_db_error(result.error());
             });

  router.add(http::verb::post, "/users",
             [&db](const RequestContext& ctx) -> asio::awaitable<Response> {
               auto input = glz::read_json<UserInput>(ctx.req.body());
               if (!input) {
                 co_return error_response(http::status::bad_request,
                                          glz::format_error(input.error(), ctx.req.body()));
               }
               auto result = co_await db.run(
                   [input = std::move(*input)](relx::PostgreSQLConnection& conn) -> DbResult<User> {
                     auto insert = relx::query::insert_into(users)
                                       .columns(users.name, users.email, users.bio)
                                       .values(input.name, input.email, input.bio)
                                       .returning(users.id, users.name, users.email, users.bio);
                     auto created = conn.execute<User>(insert);
                     if (!created) {
                       return std::unexpected(to_db_error(created.error()));
                     }
                     return std::move(*created);
                   });
               co_return result ? json_response(http::status::created, *result)
                                : from_db_error(result.error());
             });

  router.add(http::verb::put, "/users/:id",
             [&db](const RequestContext& ctx) -> asio::awaitable<Response> {
               auto id = parse_int(ctx.param("id"));
               if (!id) {
                 co_return error_response(http::status::bad_request, "id must be an integer");
               }
               auto input = glz::read_json<UserInput>(ctx.req.body());
               if (!input) {
                 co_return error_response(http::status::bad_request,
                                          glz::format_error(input.error(), ctx.req.body()));
               }
               auto result = co_await db.run(
                   [id = *id,
                    input = std::move(*input)](relx::PostgreSQLConnection& conn) -> DbResult<User> {
                     // Existence check first so a missing row is a clean 404
                     if (auto existing = fetch_user(conn, id); !existing) {
                       return existing;
                     }
                     auto update = relx::query::update(users)
                                       .set(users.name, input.name)
                                       .set(users.email, input.email)
                                       .set(users.bio, input.bio)
                                       .where(users.id == id);
                     if (auto updated = conn.execute(update); !updated) {
                       return std::unexpected(to_db_error(updated.error()));
                     }
                     return fetch_user(conn, id);
                   });
               co_return result ? json_response(http::status::ok, *result)
                                : from_db_error(result.error());
             });

  router.add(http::verb::delete_, "/users/:id",
             [&db](const RequestContext& ctx) -> asio::awaitable<Response> {
               auto id = parse_int(ctx.param("id"));
               if (!id) {
                 co_return error_response(http::status::bad_request, "id must be an integer");
               }
               auto result = co_await db.run(
                   [id = *id](relx::PostgreSQLConnection& conn) -> DbResult<void> {
                     // DeleteQuery has no RETURNING support (yet), so 404 via existence check
                     if (auto existing = fetch_user(conn, id); !existing) {
                       return std::unexpected(existing.error());
                     }
                     auto del = relx::query::delete_from(users).where(users.id == id);
                     if (auto deleted = conn.execute(del); !deleted) {
                       return std::unexpected(to_db_error(deleted.error()));
                     }
                     return {};
                   });
               if (!result) {
                 co_return from_db_error(result.error());
               }
               Response res{http::status::no_content, 11};
               co_return res;
             });
}

}  // namespace crud
