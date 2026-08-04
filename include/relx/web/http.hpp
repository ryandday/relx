#pragma once

#include "error.hpp"

#include <string>
#include <utility>

#include <boost/beast/http.hpp>

namespace relx::web {

namespace http = boost::beast::http;

using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

inline Response make_response(http::status status, std::string json_body) {
  Response res{status, 11};
  res.set(http::field::content_type, "application/json");
  res.body() = std::move(json_body);
  return res;
}

inline Response error_response(http::status status, std::string_view message) {
  return make_response(status, std::string(R"({"error":")") + std::string(message) + R"("})");
}

inline Response error_response(const ApiError& error) {
  return error_response(static_cast<http::status>(error.status), error.message);
}

inline Response no_content() {
  return Response{http::status::no_content, 11};
}

}  // namespace relx::web
