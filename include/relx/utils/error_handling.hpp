#pragma once

#include "../connection/connection.hpp"
#include "../query/core.hpp"
#include "../results/result.hpp"

#include <expected>
#include <format>
#include <source_location>
#include <stdexcept>
#include <string>

namespace relx {

/**
 * @brief Base exception class for relx errors
 */
class RelxException : public std::runtime_error {
public:
  explicit RelxException(const std::string& message,
                         const std::source_location& location = std::source_location::current())
      : std::runtime_error(
            std::format("[{}:{}] {}", location.file_name(), location.line(), message)) {}
};

/**
 * @brief Format a ConnectionError for exception messages
 */
inline std::string format_error(const connection::ConnectionError& error) {
  return std::format("Connection error: {} (Code: {})", error.message, error.error_code);
}

/**
 * @brief Format a QueryError for exception messages
 */
inline std::string format_error(const query::QueryError& error) {
  return std::format("Query error: {}", error.message);
}

/**
 * @brief Format a ResultError for exception messages
 */
inline std::string format_error(const result::ResultError& error) {
  return std::format("Result processing error: {}", error.message);
}

/**
 * @brief Function to extract a value from an expected or throw on error
 *
 * @tparam T Type of the value in std::expected
 * @tparam E Type of the error in std::expected
 * @param result The std::expected object to check
 * @param context Optional context message to include in the exception
 * @return T& Reference to the contained value if no error
 * @throws RelxException if the expected contains an error
 */
template <typename T, typename E>
T& value_or_throw(std::expected<T, E>& result, const std::string& context = "",
                  const std::source_location& location = std::source_location::current()) {
  if (!result) {
    std::string message;
    if (!context.empty()) {
      message = std::format("{}: ", context);
    }
    message += format_error(result.error());
    throw RelxException(message, location);
  }
  return result.value();
}

/**
 * @brief Overload for rvalue std::expected
 */
template <typename T, typename E>
T value_or_throw(std::expected<T, E>&& result, const std::string& context = "",
                 const std::source_location& location = std::source_location::current()) {
  if (!result) {
    std::string message;
    if (!context.empty()) {
      message = std::format("{}: ", context);
    }
    message += format_error(result.error());
    throw RelxException(message, location);
  }
  return std::move(result.value());
}

/**
 * @brief Function to check and throw on error with no return value
 */
template <typename E>
void throw_if_failed(const std::expected<void, E>& result, const std::string& context = "",
                     const std::source_location& location = std::source_location::current()) {
  if (!result) {
    std::string message;
    if (!context.empty()) {
      message = std::format("{}: ", context);
    }
    message += format_error(result.error());
    throw RelxException(message, location);
  }
}

}  // namespace relx