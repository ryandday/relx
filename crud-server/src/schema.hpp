#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include <boost/uuid/uuid.hpp>
#include <relx/schema.hpp>
#include <relx/web/projection.hpp>

namespace app {

using time_point = std::chrono::system_clock::time_point;
using uuid = boost::uuids::uuid;

// clang-format off: annotation syntax is not yet understood by clang-format

struct [[=relx::table("app_users")]] User {
  [[=relx::ann::pk, =relx::ann::identity]] int64_t id;
  std::string name;
  [[=relx::ann::unique]] std::string email;
};

struct [[=relx::table("app_events")]] Event {
  [[=relx::ann::pk, =relx::default_sql<"gen_random_uuid()">{}]] uuid id;
  [[=relx::ann::fk<^^User::id>]] int64_t owner_id;
  std::string title;
  std::optional<std::string> description;
  std::optional<std::string> location;
  [[=relx::default_value<false>{}]] bool is_all_day;
  time_point start_time;
  std::optional<time_point> end_time;
  [[=relx::default_sql<"now()">{}]] time_point created_at;
};

struct [[=relx::table("app_event_invites"),
        =relx::ann::composite_unique("event_id", "user_id")]] EventInvite {
  [[=relx::ann::pk, =relx::default_sql<"gen_random_uuid()">{}]] uuid id;
  [[=relx::ann::fk<^^Event::id>]] uuid event_id;
  [[=relx::ann::fk<^^User::id>]] int64_t user_id;
};

inline constexpr auto users = relx::t<User>;
inline constexpr auto events = relx::t<Event>;
inline constexpr auto invites = relx::t<EventInvite>;

// Request DTOs: hand-written, compile-time-checked against their tables

struct [[=relx::web::projects<User>]] SignupInput {
  std::string name;
  std::string email;
};

struct [[=relx::web::projects<Event>]] EventInput {
  std::string title;
  time_point start_time;
  std::optional<time_point> end_time;
  std::optional<std::string> description;
  std::optional<std::string> location;
  std::optional<bool> is_all_day;
};

struct [[=relx::web::projects<EventInvite>]] InviteInput {
  int64_t user_id;
};

// clang-format on

/// JWT claims this service issues
struct Claims {
  int64_t sub{};
  long long exp{};
};

struct TokenResponse {
  std::string token;
  User user;
};

}  // namespace app
