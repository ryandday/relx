#pragma once

#include "error.hpp"
#include "http.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace relx::web {

/// @brief The bearer token from an Authorization header, if present
inline std::optional<std::string_view> bearer_token(const Request& req) {
  auto it = req.find(http::field::authorization);
  if (it == req.end()) {
    return std::nullopt;
  }
  std::string_view value{it->value()};
  constexpr std::string_view prefix = "Bearer ";
  if (!value.starts_with(prefix)) {
    return std::nullopt;
  }
  return value.substr(prefix.size());
}

/// @brief Authenticate a request with a pluggable verifier:
/// `verify(token) -> ApiResult<Claims>` for any claims type. A missing or
/// malformed Authorization header is a 401 without calling the verifier.
template <typename Verifier>
auto authenticate(const Request& req, Verifier&& verify) -> decltype(verify(std::string_view{})) {
  auto token = bearer_token(req);
  if (!token) {
    return std::unexpected(unauthorized("missing bearer token"));
  }
  return std::forward<Verifier>(verify)(*token);
}

}  // namespace relx::web
