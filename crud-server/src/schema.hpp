#pragma once

#include <optional>
#include <string>

#include <relx/schema.hpp>

namespace crud {

// clang-format off: annotation syntax is not yet understood by clang-format

/// One struct is the relx schema definition, the query result DTO, and the
/// glaze JSON response shape.
struct [[=relx::table("crud_users")]] User {
  [[=relx::ann::pk, =relx::ann::identity]] int id;
  std::string name;
  [[=relx::ann::unique]] std::string email;
  std::optional<std::string> bio;
};

// clang-format on

inline constexpr auto users = relx::t<User>;

/// Request body for POST /users and PUT /users/:id (id is database-assigned)
struct UserInput {
  std::string name;
  std::string email;
  std::optional<std::string> bio;
};

}  // namespace crud
