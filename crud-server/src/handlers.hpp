#pragma once

#include "schema.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/uuid/string_generator.hpp>
#include <relx/query.hpp>
#include <relx/web.hpp>
#include <relx/web/jwt_hs256.hpp>

namespace app {

namespace web = relx::web;
namespace rq = relx::query;

inline std::optional<uuid> parse_uuid(std::string_view s) {
  try {
    return boost::uuids::string_generator{}(std::string(s));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

inline std::string make_token(int64_t user_id, std::string_view secret) {
  const auto exp = std::chrono::duration_cast<std::chrono::seconds>(
                       (std::chrono::system_clock::now() + std::chrono::hours(24))
                           .time_since_epoch())
                       .count();
  return web::jwt::sign(
      R"({"sub":)" + std::to_string(user_id) + R"(,"exp":)" + std::to_string(exp) + "}", secret);
}

/// Authenticated user id from the bearer token, or a 401
inline web::ApiResult<int64_t> current_user(const web::RequestContext& ctx,
                                            std::string_view secret) {
  return web::authenticate(ctx.req, [&](std::string_view token) -> web::ApiResult<int64_t> {
    auto payload = web::jwt::verify(token, secret);
    if (!payload) {
      return std::unexpected(payload.error());
    }
    Claims claims{};
    if (glz::read<glz::opts{.error_on_unknown_keys = false}>(claims, *payload) ||
        claims.sub == 0) {
      return std::unexpected(web::unauthorized("malformed claims"));
    }
    return claims.sub;
  });
}

inline auto select_events() {
  return rq::select(events.id, events.owner_id, events.title, events.description, events.location,
                    events.is_all_day, events.start_time, events.end_time, events.created_at)
      .from(events);
}

/// Business validation for create/update bodies (422, like the reference backend)
inline web::ApiResult<void> validate_event(const EventInput& input) {
  if (input.title.empty()) {
    return std::unexpected(web::ApiError{.status = 422, .message = "title must not be empty"});
  }
  if (input.end_time && *input.end_time <= input.start_time) {
    return std::unexpected(
        web::ApiError{.status = 422, .message = "end_time must be after start_time"});
  }
  return {};
}

inline void register_routes(web::Router& router, web::Db& db, std::string jwt_secret) {
  // ---- auth ----------------------------------------------------------------

  router.add(web::http::verb::post, "/auth/signup",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto input = web::read_body<SignupInput>(ctx.req);
               if (!input) {
                 co_return web::error_response(input.error());
               }
               auto result = co_await db.run(
                   [input = std::move(*input)](
                       relx::PostgreSQLConnection& conn) -> web::ApiResult<User> {
                     auto insert = rq::insert_into(users)
                                       .columns(users.name, users.email)
                                       .values(input.name, input.email)
                                       .returning(users.id, users.name, users.email);
                     auto created = conn.execute<User>(insert);
                     if (!created) {
                       return std::unexpected(web::from_connection_error(created.error()));
                     }
                     return std::move(*created);
                   });
               if (!result) {
                 co_return web::error_response(result.error());
               }
               co_return web::json_response(
                   web::http::status::created,
                   TokenResponse{.token = make_token(result->id, jwt_secret),
                                 .user = std::move(*result)});
             });

  router.add(web::http::verb::post, "/auth/token",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               struct EmailInput {
                 std::string email;
               };
               auto input = web::read_body<EmailInput>(ctx.req);
               if (!input) {
                 co_return web::error_response(input.error());
               }
               auto result = co_await db.run(
                   [email = std::move(input->email)](
                       relx::PostgreSQLConnection& conn) -> web::ApiResult<User> {
                     return web::one_or_404<User>(
                         conn,
                         rq::select(users.id, users.name, users.email)
                             .from(users)
                             .where(users.email == email),
                         "user");
                   });
               if (!result) {
                 co_return web::error_response(result.error());
               }
               co_return web::json_response(
                   web::http::status::ok,
                   TokenResponse{.token = make_token(result->id, jwt_secret),
                                 .user = std::move(*result)});
             });

  router.add(web::http::verb::get, "/users/me",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto result = co_await db.run(
                   [id = *user_id](relx::PostgreSQLConnection& conn) -> web::ApiResult<User> {
                     return web::one_or_404<User>(conn,
                                                  rq::select(users.id, users.name, users.email)
                                                      .from(users)
                                                      .where(users.id == id),
                                                  "user");
                   });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  // ---- events --------------------------------------------------------------

  // Literal route must be registered before /events/:id
  router.add(web::http::verb::get, "/events/invited",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto page = web::page_params(ctx);
               if (!page) {
                 co_return web::error_response(page.error());
               }
               auto result = co_await db.run(
                   [uid = *user_id,
                    page = *page](relx::PostgreSQLConnection& conn)
                       -> web::ApiResult<std::vector<Event>> {
                     auto query = select_events()
                                      .join(invites, rq::on(events.id == invites.event_id))
                                      .where(invites.user_id == uid)
                                      .order_by(events.start_time)
                                      .limit(page.limit)
                                      .offset(page.offset);
                     auto rows = conn.execute_many<Event>(query);
                     if (!rows) {
                       return std::unexpected(web::from_connection_error(rows.error()));
                     }
                     return std::move(*rows);
                   });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  router.add(web::http::verb::get, "/events",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto page = web::page_params(ctx);
               if (!page) {
                 co_return web::error_response(page.error());
               }
               auto result = co_await db.run(
                   [uid = *user_id,
                    page = *page](relx::PostgreSQLConnection& conn)
                       -> web::ApiResult<std::vector<Event>> {
                     auto query = select_events()
                                      .where(events.owner_id == uid)
                                      .order_by(events.start_time)
                                      .limit(page.limit)
                                      .offset(page.offset);
                     auto rows = conn.execute_many<Event>(query);
                     if (!rows) {
                       return std::unexpected(web::from_connection_error(rows.error()));
                     }
                     return std::move(*rows);
                   });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  router.add(web::http::verb::post, "/events",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto input = web::read_body<EventInput>(ctx.req);
               if (!input) {
                 co_return web::error_response(input.error());
               }
               if (auto valid = validate_event(*input); !valid) {
                 co_return web::error_response(valid.error());
               }
               auto result = co_await db.run(
                   [uid = *user_id, input = std::move(*input)](
                       relx::PostgreSQLConnection& conn) -> web::ApiResult<Event> {
                     auto insert =
                         rq::insert_into(events)
                             .columns(events.owner_id, events.title, events.description,
                                      events.location, events.is_all_day, events.start_time,
                                      events.end_time)
                             .values(uid, input.title, input.description, input.location,
                                     input.is_all_day.value_or(false), input.start_time,
                                     input.end_time)
                             .returning(events.id, events.owner_id, events.title,
                                        events.description, events.location, events.is_all_day,
                                        events.start_time, events.end_time, events.created_at);
                     auto created = conn.execute<Event>(insert);
                     if (!created) {
                       return std::unexpected(web::from_connection_error(created.error()));
                     }
                     return std::move(*created);
                   });
               co_return result ? web::json_response(web::http::status::created, *result)
                                : web::error_response(result.error());
             });

  router.add(web::http::verb::get, "/events/:id",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto event_id = parse_uuid(ctx.param("id"));
               if (!event_id) {
                 co_return web::error_response(web::bad_request("id must be a uuid"));
               }
               auto result = co_await db.run(
                   [uid = *user_id,
                    eid = *event_id](relx::PostgreSQLConnection& conn) -> web::ApiResult<Event> {
                     auto event = web::one_or_404<Event>(
                         conn, select_events().where(events.id == eid), "event");
                     if (!event) {
                       return event;
                     }
                     if (event->owner_id != uid) {
                       // Not the owner: visible only when invited; 404 avoids leaking existence
                       auto invite = conn.execute_many<EventInvite>(
                           rq::select(invites.id, invites.event_id, invites.user_id)
                               .from(invites)
                               .where(invites.event_id == eid && invites.user_id == uid));
                       if (!invite) {
                         return std::unexpected(web::from_connection_error(invite.error()));
                       }
                       if (invite->empty()) {
                         return std::unexpected(web::not_found("event not found"));
                       }
                     }
                     return event;
                   });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  router.add(web::http::verb::put, "/events/:id",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto event_id = parse_uuid(ctx.param("id"));
               if (!event_id) {
                 co_return web::error_response(web::bad_request("id must be a uuid"));
               }
               auto input = web::read_body<EventInput>(ctx.req);
               if (!input) {
                 co_return web::error_response(input.error());
               }
               if (auto valid = validate_event(*input); !valid) {
                 co_return web::error_response(valid.error());
               }
               auto result = co_await db.run(
                   [uid = *user_id, eid = *event_id, input = std::move(*input)](
                       relx::PostgreSQLConnection& conn) -> web::ApiResult<Event> {
                     // Ownership folded into WHERE: non-owners get the same 404 as missing rows
                     auto update =
                         rq::update(events)
                             .set(events.title, input.title)
                             .set(events.description, input.description)
                             .set(events.location, input.location)
                             .set(events.is_all_day, input.is_all_day.value_or(false))
                             .set(events.start_time, input.start_time)
                             .set(events.end_time, input.end_time)
                             .where(events.id == eid && events.owner_id == uid)
                             .returning(events.id, events.owner_id, events.title,
                                        events.description, events.location, events.is_all_day,
                                        events.start_time, events.end_time, events.created_at);
                     auto updated = conn.execute_many<Event>(update);
                     if (!updated) {
                       return std::unexpected(web::from_connection_error(updated.error()));
                     }
                     if (updated->empty()) {
                       return std::unexpected(web::not_found("event not found"));
                     }
                     return std::move(updated->front());
                   });
               co_return result ? web::json_response(web::http::status::ok, *result)
                                : web::error_response(result.error());
             });

  router.add(web::http::verb::delete_, "/events/:id",
             [&db, jwt_secret](const web::RequestContext& ctx)
                 -> web::asio::awaitable<web::Response> {
               auto user_id = current_user(ctx, jwt_secret);
               if (!user_id) {
                 co_return web::error_response(user_id.error());
               }
               auto event_id = parse_uuid(ctx.param("id"));
               if (!event_id) {
                 co_return web::error_response(web::bad_request("id must be a uuid"));
               }
               auto result = co_await db.run(
                   [uid = *user_id,
                    eid = *event_id](relx::PostgreSQLConnection& conn) -> web::ApiResult<void> {
                     // Invites cascade is not modeled; clear them first, then the event
                     auto drop_invites =
                         rq::delete_from(invites).where(invites.event_id == eid);
                     if (auto dropped = conn.execute(drop_invites); !dropped) {
                       return std::unexpected(web::from_connection_error(dropped.error()));
                     }
                     auto del = rq::delete_from(events)
                                    .where(events.id == eid && events.owner_id == uid)
                                    .returning(events.id);
                     auto deleted = conn.execute(del);
                     if (!deleted) {
                       return std::unexpected(web::from_connection_error(deleted.error()));
                     }
                     if (deleted->empty()) {
                       return std::unexpected(web::not_found("event not found"));
                     }
                     return {};
                   });
               co_return result ? web::no_content() : web::error_response(result.error());
             });

  router.add(
      web::http::verb::post, "/events/:id/invites",
      [&db, jwt_secret](const web::RequestContext& ctx) -> web::asio::awaitable<web::Response> {
        auto user_id = current_user(ctx, jwt_secret);
        if (!user_id) {
          co_return web::error_response(user_id.error());
        }
        auto event_id = parse_uuid(ctx.param("id"));
        if (!event_id) {
          co_return web::error_response(web::bad_request("id must be a uuid"));
        }
        auto input = web::read_body<InviteInput>(ctx.req);
        if (!input) {
          co_return web::error_response(input.error());
        }
        auto result = co_await db.run(
            [uid = *user_id, eid = *event_id, invitee = input->user_id](
                relx::PostgreSQLConnection& conn) -> web::ApiResult<EventInvite> {
              auto event =
                  web::one_or_404<Event>(conn, select_events().where(events.id == eid), "event");
              if (!event) {
                return std::unexpected(event.error());
              }
              if (auto owner = web::require_owner(event->owner_id, uid, "event"); !owner) {
                return std::unexpected(owner.error());
              }
              if (invitee == uid) {
                return std::unexpected(web::conflict("cannot invite yourself"));
              }
              auto target = web::one_or_404<User>(
                  conn, rq::select(users.id, users.name, users.email)
                            .from(users)
                            .where(users.id == invitee),
                  "user");
              if (!target) {
                return std::unexpected(target.error());
              }
              auto insert = rq::insert_into(invites)
                                .columns(invites.event_id, invites.user_id)
                                .values(eid, invitee)
                                .returning(invites.id, invites.event_id, invites.user_id);
              auto created = conn.execute<EventInvite>(insert);
              if (!created) {
                return std::unexpected(web::from_connection_error(created.error()));
              }
              return std::move(*created);
            });
        co_return result ? web::json_response(web::http::status::created, *result)
                         : web::error_response(result.error());
      });
}

}  // namespace app
