#pragma once

#include <expected>
#include <string>

namespace relx::web {

/// @brief Error shaped for an HTTP response. Handlers and helpers pass this
/// through std::expected; the HTTP layer renders it as a JSON error body.
struct ApiError {
  int status = 500;
  std::string message;
};

template <typename T>
using ApiResult = std::expected<T, ApiError>;

inline ApiError bad_request(std::string message) {
  return {.status = 400, .message = std::move(message)};
}
inline ApiError unauthorized(std::string message = "unauthorized") {
  return {.status = 401, .message = std::move(message)};
}
inline ApiError forbidden(std::string message = "forbidden") {
  return {.status = 403, .message = std::move(message)};
}
inline ApiError not_found(std::string message = "not found") {
  return {.status = 404, .message = std::move(message)};
}
inline ApiError conflict(std::string message) {
  return {.status = 409, .message = std::move(message)};
}

}  // namespace relx::web
