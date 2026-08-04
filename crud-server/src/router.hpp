#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>

namespace crud {

namespace asio = boost::asio;
namespace http = boost::beast::http;

using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

/// A matched request: the Beast request plus captured path parameters
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
};

using Handler = std::function<asio::awaitable<Response>(const RequestContext&)>;

inline Response make_response(http::status status, std::string json_body) {
  Response res{status, 11};
  res.set(http::field::content_type, "application/json");
  res.body() = std::move(json_body);
  return res;
}

inline Response error_response(http::status status, std::string_view message) {
  return make_response(status, std::string(R"({"error":")") + std::string(message) + R"("})");
}

/// Minimal segment-based router: literal segments match exactly, ":name"
/// segments capture the value as a path parameter.
class Router {
public:
  void add(http::verb method, std::string_view pattern, Handler handler) {
    routes_.push_back({method, split(pattern), std::move(handler)});
  }

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
      co_return co_await route.handler(ctx);
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
};

}  // namespace crud
