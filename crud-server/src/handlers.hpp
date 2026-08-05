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

using web::unwrap;
template <typename T>
using Aw = web::asio::awaitable<T>;

inline uuid uuid_param(const web::RequestContext& ctx, std::string_view name) {
  try {
    return boost::uuids::string_generator{}(std::string(ctx.param(name)));
  } catch (const std::exception&) {
    throw web::ApiException(web::bad_request(std::string(name) + " must be a uuid"));
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

/// Business validation for create/update bodies (422, like the reference backend)
inline void validate_event(const EventInput& input) {
  if (input.title.empty()) {
    throw web::ApiException({.status = 422, .message = "title must not be empty"});
  }
  if (input.end_time && *input.end_time <= input.start_time) {
    throw web::ApiException({.status = 422, .message = "end_time must be after start_time"});
  }
}

inline auto select_user() {
  return rq::select_all(users);
}

inline void register_routes(web::Router& router, web::Db& db, std::string jwt_secret) {
  // ---- auth (public) -------------------------------------------------------

  router.post("/auth/signup", [&db, jwt_secret](const auto& ctx) -> Aw<web::Response> {
    auto input = unwrap(web::read_body<SignupInput>(ctx.req));
    auto user = unwrap(co_await db.fetch_one<User>(rq::insert_into(users)
                                                       .columns(users.name, users.email)
                                                       .values(input.name, input.email)
                                                       .returning_all()));
    co_return web::created(TokenResponse{.token = make_token(user.id, jwt_secret), .user = user});
  });

  router.post("/auth/token", [&db, jwt_secret](const auto& ctx) -> Aw<web::Response> {
    struct EmailInput {
      std::string email;
    };
    auto input = unwrap(web::read_body<EmailInput>(ctx.req));
    auto user = unwrap(
        co_await db.fetch_one<User>(select_user().where(users.email == input.email), "user"));
    co_return web::ok(TokenResponse{.token = make_token(user.id, jwt_secret), .user = user});
  });

  // ---- authenticated routes ------------------------------------------------

  auto api = web::authed<int64_t>(
      router, [jwt_secret](const web::RequestContext& ctx) { return current_user(ctx, jwt_secret); });

  api.get("/users/me", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    co_return web::ok(
        unwrap(co_await db.fetch_one<User>(select_user().where(users.id == uid), "user")));
  });

  // Literal route must be registered before /events/:id
  api.get("/events/invited", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto page = unwrap(web::page_params(ctx));
    co_return web::ok(unwrap(co_await db.fetch_all<Event>(
        rq::select_all(events)
            .join(invites, rq::on(events.id == invites.event_id))
            .where(invites.user_id == uid)
            .order_by(events.start_time)
            .limit(page.limit)
            .offset(page.offset))));
  });

  api.get("/events", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto page = unwrap(web::page_params(ctx));
    co_return web::ok(unwrap(co_await db.fetch_all<Event>(rq::select_all(events)
                                                              .where(events.owner_id == uid)
                                                              .order_by(events.start_time)
                                                              .limit(page.limit)
                                                              .offset(page.offset))));
  });

  api.post("/events", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto input = unwrap(web::read_body<EventInput>(ctx.req));
    validate_event(input);
    co_return web::created(unwrap(co_await db.fetch_one<Event>(
        rq::insert_into(events)
            .columns(events.owner_id, events.title, events.description, events.location,
                     events.is_all_day, events.start_time, events.end_time)
            .values(uid, input.title, input.description, input.location,
                    input.is_all_day.value_or(false), input.start_time, input.end_time)
            .returning_all())));
  });

  api.get("/events/:id", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto eid = uuid_param(ctx, "id");
    auto event = unwrap(co_await db.fetch_one<Event>(
        rq::select_all(events).where(events.id == eid), "event"));
    if (event.owner_id != uid) {
      // Not the owner: visible only when invited; 404 avoids leaking existence
      unwrap(co_await db.fetch_one<EventInvite>(
                 rq::select_all(invites).where(invites.event_id == eid && invites.user_id == uid)),
             web::not_found("event not found"));
    }
    co_return web::ok(event);
  });

  api.put("/events/:id", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto eid = uuid_param(ctx, "id");
    auto input = unwrap(web::read_body<EventInput>(ctx.req));
    validate_event(input);
    // Ownership folded into WHERE: non-owners get the same 404 as missing rows
    co_return web::ok(unwrap(co_await db.fetch_one<Event>(
        rq::update(events)
            .set(events.title, input.title)
            .set(events.description, input.description)
            .set(events.location, input.location)
            .set(events.is_all_day, input.is_all_day.value_or(false))
            .set(events.start_time, input.start_time)
            .set(events.end_time, input.end_time)
            .where(events.id == eid && events.owner_id == uid)
            .returning_all(),
        "event")));
  });

  api.del("/events/:id", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto eid = uuid_param(ctx, "id");
    // Multi-statement: invites cascade is not modeled, so clear them in the same run
    unwrap(co_await db.run([eid, uid](relx::PostgreSQLConnection& conn) -> web::ApiResult<void> {
      unwrap(conn.execute(rq::delete_from(invites).where(invites.event_id == eid)));
      auto deleted = unwrap(conn.execute(rq::delete_from(events)
                                             .where(events.id == eid && events.owner_id == uid)
                                             .returning(events.id)));
      if (deleted.empty()) {
        return std::unexpected(web::not_found("event not found"));
      }
      return {};
    }));
    co_return web::no_content();
  });

  api.post("/events/:id/invites", [&db](const auto& ctx, int64_t uid) -> Aw<web::Response> {
    auto eid = uuid_param(ctx, "id");
    auto input = unwrap(web::read_body<InviteInput>(ctx.req));
    auto invite = unwrap(co_await db.run(
        [eid, uid, invitee = input.user_id](
            relx::PostgreSQLConnection& conn) -> web::ApiResult<EventInvite> {
          auto event = unwrap(
              web::one_or_404<Event>(conn, rq::select_all(events).where(events.id == eid), "event"));
          web::require_owner(event.owner_id, uid, "event");
          if (invitee == uid) {
            return std::unexpected(web::conflict("cannot invite yourself"));
          }
          unwrap(web::one_or_404<User>(conn, select_user().where(users.id == invitee), "user"));
          auto created = conn.execute<EventInvite>(rq::insert_into(invites)
                                                       .columns(invites.event_id, invites.user_id)
                                                       .values(eid, invitee)
                                                       .returning_all());
          if (!created) {
            return std::unexpected(web::from_connection_error(created.error()));
          }
          return std::move(*created);
        }));
    co_return web::created(invite);
  });
}

}  // namespace app
