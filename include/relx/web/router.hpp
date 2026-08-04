#pragma once

#include "http.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>

namespace relx::web {

namespace asio = boost::asio;

/// @brief A matched request: the Beast request plus captured path parameters
struct RequestContext {
  const Request& req;
  std::vector<std::pair<std::string, std::string>> path_params;

  std::string_view param(std::string_view name) const {
    for (const auto& [key, value] : path_params) {
      if (key == name) {
        return value;
      }
    }
    return {};
  }

  /// @brief Value of a query-string parameter (?limit=10), or empty
  std::string_view query_param(std::string_view name) const {
    std::string_view target{req.target()};
    auto pos = target.find('?');
    if (pos == std::string_view::npos) {
      return {};
    }
    std::string_view qs = target.substr(pos + 1);
    while (!qs.empty()) {
      auto amp = qs.find('&');
      std::string_view pair = qs.substr(0, amp);
      auto eq = pair.find('=');
      if (eq != std::string_view::npos && pair.substr(0, eq) == name) {
        return pair.substr(eq + 1);
      }
      if (amp == std::string_view::npos) {
        break;
      }
      qs = qs.substr(amp + 1);
    }
    return {};
  }
};

using Handler = std::function<asio::awaitable<Response>(const RequestContext&)>;

/// @brief Renders an exception escaping a handler into a Response
using ErrorHandler = std::function<Response(std::exception_ptr, const Request&)>;

/// @brief Default error rendering: ApiException keeps its status, anything else is a 500
inline Response default_error_handler(std::exception_ptr error, const Request&) {
  try {
    std::rethrow_exception(error);
  } catch (const ApiException& e) {
    return error_response(e.error());
  } catch (const std::exception& e) {
    return error_response(http::status::internal_server_error, e.what());
  } catch (...) {
    return error_response(http::status::internal_server_error, "unknown error");
  }
}

/// @brief Minimal segment-based router: literal segments match exactly, ":name"
/// segments capture the value as a path parameter.
class Router {
public:
  void add(http::verb method, std::string_view pattern, Handler handler) {
    routes_.push_back({method, split(pattern), std::move(handler)});
  }

  // Verb shorthands
  void get(std::string_view pattern, Handler h) { add(http::verb::get, pattern, std::move(h)); }
  void post(std::string_view pattern, Handler h) { add(http::verb::post, pattern, std::move(h)); }
  void put(std::string_view pattern, Handler h) { add(http::verb::put, pattern, std::move(h)); }
  void del(std::string_view pattern, Handler h) { add(http::verb::delete_, pattern, std::move(h)); }

  /// @brief Replace how exceptions escaping handlers are rendered (framework-style
  /// generic error handler). The default maps ApiException to its status and
  /// anything else to a 500.
  void set_error_handler(ErrorHandler handler) { error_handler_ = std::move(handler); }

  asio::awaitable<Response> dispatch(const Request& req) const {
    std::string_view target{req.target()};
    if (auto query_start = target.find('?'); query_start != std::string_view::npos) {
      target = target.substr(0, query_start);
    }
    const auto segments = split(target);

    bool path_matched = false;
    for (const auto& route : routes_) {
      RequestContext ctx{.req = req, .path_params = {}};
      if (!match(route.segments, segments, ctx.path_params)) {
        continue;
      }
      path_matched = true;
      if (route.method != req.method()) {
        continue;
      }
      try {
        co_return co_await route.handler(ctx);
      } catch (...) {
        co_return error_handler_(std::current_exception(), req);
      }
    }
    co_return path_matched ? error_response(http::status::method_not_allowed, "method not allowed")
                           : error_response(http::status::not_found, "no such route");
  }

private:
  struct Route {
    http::verb method;
    std::vector<std::string> segments;
    Handler handler;
  };

  static std::vector<std::string> split(std::string_view path) {
    std::vector<std::string> segments;
    for (size_t pos = 0; pos < path.size();) {
      const size_t next = path.find('/', pos);
      const size_t end = (next == std::string_view::npos) ? path.size() : next;
      if (end > pos) {
        segments.emplace_back(path.substr(pos, end - pos));
      }
      pos = end + 1;
    }
    return segments;
  }

  static bool match(const std::vector<std::string>& pattern, const std::vector<std::string>& path,
                    std::vector<std::pair<std::string, std::string>>& params) {
    if (pattern.size() != path.size()) {
      return false;
    }
    for (size_t i = 0; i < pattern.size(); ++i) {
      if (!pattern[i].empty() && pattern[i].front() == ':') {
        params.emplace_back(pattern[i].substr(1), path[i]);
      } else if (pattern[i] != path[i]) {
        return false;
      }
    }
    return true;
  }

  std::vector<Route> routes_;
  ErrorHandler error_handler_ = default_error_handler;
};

}  // namespace relx::web
