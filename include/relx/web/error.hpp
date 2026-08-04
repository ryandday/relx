#pragma once

#include <concepts>
#include <exception>
#include <expected>
#include <string>
#include <utility>

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

/// @brief Exception carrying an ApiError through handler coroutines to the
/// router's error handler, which renders it with its status intact.
/// The status-preserving sibling of relx::value_or_throw/RelxException.
class ApiException : public std::exception {
public:
  explicit ApiException(ApiError error) : error_(std::move(error)) {}

  const ApiError& error() const noexcept { return error_; }
  const char* what() const noexcept override { return error_.message.c_str(); }

private:
  ApiError error_;
};

/// @brief Extract the value from an ApiResult or throw its error (status preserved)
template <typename T>
T unwrap(ApiResult<T> result) {
  if (!result) {
    throw ApiException(std::move(result).error());
  }
  return *std::move(result);
}

inline void unwrap(ApiResult<void> result) {
  if (!result) {
    throw ApiException(std::move(result).error());
  }
}

/// @brief Extract the value or throw a caller-chosen error instead of the result's own
template <typename T>
T unwrap(ApiResult<T> result, ApiError override_error) {
  if (!result) {
    throw ApiException(std::move(override_error));
  }
  return *std::move(result);
}

/// @brief Extract the value from any other expected (ConnectionResult, ...) or
/// throw its message as a 500 — infrastructure failures are server errors
template <typename T, typename E>
  requires(!std::same_as<E, ApiError>) && requires(const E& e) {
    { e.message } -> std::convertible_to<std::string>;
  }
T unwrap(std::expected<T, E> result) {
  if (!result) {
    throw ApiException({.status = 500, .message = result.error().message});
  }
  if constexpr (!std::is_void_v<T>) {
    return *std::move(result);
  }
}

}  // namespace relx::web
