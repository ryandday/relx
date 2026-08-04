#pragma once

#include "schema.hpp"

#include <charconv>
#include <optional>
#include <string_view>

#include <relx/query.hpp>
#include <relx/web.hpp>

namespace crud {

namespace web = relx::web;

inline std::optional<int> parse_int(std::string_view s) {
  int value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return std::nullopt;
  }
  return value;
}

inline auto select_user_columns() {
  return relx::query::select(users.id, users.name, users.email, users.bio).from(users);
}

inline web::ApiResult<User> fetch_user(relx::PostgreSQLConnection& conn, int id) {
  auto rows = conn.execute_many<User>(select_user_columns().where(users.id == id));
  if (!rows) {
    return std::unexpected(web::from_connection_error(rows.error()));
  }
  if (rows->empty()) {
    return std::unexpected(web::not_found("user not found"));
  }
  return rows->front();
}

inline void register_user_routes(web::Router& router, web::Db& db) {
  router.add(web::http::verb::get, "/users",
             [&db](const web::RequestContext&) -> web::asio::awaitable<web::Response> {
               auto result = co_await db.run(
                   [](relx::PostgreSQLConnection& conn) -> web::ApiResult<std::vector<User>> {
                     auto rows = conn.execute_many<User>(select_user_columns().order_by(users.id));
                     if (!rows) {
                       return std::unexpected(web::from_connection_error(rows.error()));
                     }
                     return std::move(*rows);
                   });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  router.add(web::http::verb::get, "/users/:id",
             [&db](const web::RequestContext& ctx) -> web::asio::awaitable<web::Response> {
               auto id = parse_int(ctx.param("id"));
               if (!id) {
                 co_return web::error_response(web::bad_request("id must be an integer"));
               }
               auto result = co_await db.run(
                   [id = *id](relx::PostgreSQLConnection& conn) { return fetch_user(conn, id); });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  router.add(
      web::http::verb::post, "/users",
      [&db](const web::RequestContext& ctx) -> web::asio::awaitable<web::Response> {
        auto input = web::read_body<UserInput>(ctx.req);
        if (!input) {
          co_return web::error_response(input.error());
        }
        auto result = co_await db.run(
            [input = std::move(*input)](relx::PostgreSQLConnection& conn) -> web::ApiResult<User> {
              auto insert = relx::query::insert_into(users)
                                .columns(users.name, users.email, users.bio)
                                .values(input.name, input.email, input.bio)
                                .returning(users.id, users.name, users.email, users.bio);
              auto created = conn.execute<User>(insert);
              if (!created) {
                return std::unexpected(web::from_connection_error(created.error()));
              }
              return std::move(*created);
            });
        co_return result ? web::json_response(web::http::status::created, *result)
                         : web::error_response(result.error());
      });

  router.add(
      web::http::verb::put, "/users/:id",
      [&db](const web::RequestContext& ctx) -> web::asio::awaitable<web::Response> {
        auto id = parse_int(ctx.param("id"));
        if (!id) {
          co_return web::error_response(web::bad_request("id must be an integer"));
        }
        auto input = web::read_body<UserInput>(ctx.req);
        if (!input) {
          co_return web::error_response(input.error());
        }
        auto result = co_await db.run(
            [id = *id,
             input = std::move(*input)](relx::PostgreSQLConnection& conn) -> web::ApiResult<User> {
              auto update = relx::query::update(users)
                                .set(users.name, input.name)
                                .set(users.email, input.email)
                                .set(users.bio, input.bio)
                                .where(users.id == id)
                                .returning(users.id, users.name, users.email, users.bio);
              auto updated = conn.execute_many<User>(update);
              if (!updated) {
                return std::unexpected(web::from_connection_error(updated.error()));
              }
              if (updated->empty()) {
                return std::unexpected(web::not_found("user not found"));
              }
              return updated->front();
            });
        co_return result ? web::json_response(web::http::status::ok, *result)
                         : web::error_response(result.error());
      });

  router.add(
      web::http::verb::delete_, "/users/:id",
      [&db](const web::RequestContext& ctx) -> web::asio::awaitable<web::Response> {
        auto id = parse_int(ctx.param("id"));
        if (!id) {
          co_return web::error_response(web::bad_request("id must be an integer"));
        }
        auto result = co_await db.run(
            [id = *id](relx::PostgreSQLConnection& conn) -> web::ApiResult<void> {
              auto del = relx::query::delete_from(users).where(users.id == id).returning(users.id);
              auto deleted = conn.execute(del);
              if (!deleted) {
                return std::unexpected(web::from_connection_error(deleted.error()));
              }
              if (deleted->empty()) {
                return std::unexpected(web::not_found("user not found"));
              }
              return {};
            });
        co_return result ? web::no_content() : web::error_response(result.error());
      });
}

}  // namespace crud
