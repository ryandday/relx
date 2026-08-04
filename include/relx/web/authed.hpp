#pragma once

#include "error.hpp"
#include "router.hpp"

#include <functional>
#include <string_view>
#include <utility>

#include <boost/asio/awaitable.hpp>

namespace relx::web {

/// @brief Route group with authentication applied before every handler.
///
/// The auth function resolves a request to an application identity (user id,
/// claims struct, ...); its failure — typically a 401 — short-circuits the
/// handler. Handlers receive the identity as a second argument:
///
/// ```cpp
/// auto api = relx::web::authed<int64_t>(router, [&](const auto& ctx) {
///   return current_user(ctx, jwt_secret);
/// });
/// api.get("/events/:id", [&](const auto& ctx, int64_t uid) -> Aw<Response> { ... });
/// ```
template <typename Identity>
class AuthedRoutes {
public:
  using AuthFn = std::function<ApiResult<Identity>(const RequestContext&)>;

  AuthedRoutes(Router& router, AuthFn auth) : router_(router), auth_(std::move(auth)) {}

  template <typename H>
  void add(http::verb method, std::string_view pattern, H handler) {
    router_.add(method, pattern,
                [auth = auth_, handler = std::move(handler)](
                    const RequestContext& ctx) -> asio::awaitable<Response> {
                  Identity identity = unwrap(auth(ctx));
                  co_return co_await handler(ctx, identity);
                });
  }

  template <typename H>
  void get(std::string_view pattern, H h) {
    add(http::verb::get, pattern, std::move(h));
  }
  template <typename H>
  void post(std::string_view pattern, H h) {
    add(http::verb::post, pattern, std::move(h));
  }
  template <typename H>
  void put(std::string_view pattern, H h) {
    add(http::verb::put, pattern, std::move(h));
  }
  template <typename H>
  void del(std::string_view pattern, H h) {
    add(http::verb::delete_, pattern, std::move(h));
  }

private:
  Router& router_;
  AuthFn auth_;
};

/// @brief Create an authenticated route group over `router`
template <typename Identity, typename AuthFn>
AuthedRoutes<Identity> authed(Router& router, AuthFn auth) {
  return AuthedRoutes<Identity>(router, std::move(auth));
}

}  // namespace relx::web
