#pragma once

#include "error.hpp"
#include "http.hpp"

#include <string>
#include <string_view>

#include <glaze/glaze.hpp>

namespace relx::web {

/// @brief Serialize a value as a JSON response with the given status
template <typename T>
Response json_response(http::status status, const T& value) {
  auto json = glz::write_json(value);
  if (!json) {
    return error_response(http::status::internal_server_error, "serialization failed");
  }
  return make_response(status, std::move(*json));
}

/// @brief Parse a request body into T. Non-optional fields are required;
/// missing or mistyped fields produce a 400 with glaze's diagnostic.
template <typename T>
ApiResult<T> read_body(const Request& req) {
  T value{};
  auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(
      value, req.body());
  if (ec) {
    return std::unexpected(bad_request(glz::format_error(ec, req.body())));
  }
  return value;
}

}  // namespace relx::web
