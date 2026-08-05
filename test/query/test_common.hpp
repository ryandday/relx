#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <relx/query.hpp>
#include <relx/schema.hpp>

// Common test tables used across query tests.
// Each annotated struct is paired with its table object; tests use the table object
// (test_tables::users) wherever they used to declare an instance.
namespace test_tables {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// User table
struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
  [[=relx::ann::unique]] std::string email;
  int age;
  std::string created_at;
  bool is_active;
  std::optional<std::string> bio;
  int login_count;
};
inline constexpr auto users = relx::t<Users>;

// Post table with foreign key to users
struct [[=relx::table("posts")]] Posts {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  std::string title;
  std::string content;
  int views;
  std::string created_at;
  bool is_published;
};
inline constexpr auto posts = relx::t<Posts>;

// Comments table with foreign keys to posts and users
struct [[=relx::table("comments")]] Comments {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Posts::id>]] int post_id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  std::string content;
  std::string created_at;
  bool is_approved;
};
inline constexpr auto comments = relx::t<Comments>;

// Tags table for a many-to-many relationship with posts
struct [[=relx::table("tags")]] Tags {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string name;
};
inline constexpr auto tags = relx::t<Tags>;

// Junction table for posts <-> tags many-to-many relationship
struct [[=relx::table("post_tags"),
        =relx::ann::composite_pk("post_id", "tag_id")]] PostTags {
  [[=relx::ann::fk<^^Posts::id>]] int post_id;
  [[=relx::ann::fk<^^Tags::id>]] int tag_id;
};
inline constexpr auto post_tags = relx::t<PostTags>;

// User profile table with one-to-one relationship with users
struct [[=relx::table("user_profiles")]] UserProfiles {
  [[=relx::ann::pk, =relx::ann::fk<^^Users::id>]] int user_id;
  std::optional<std::string> profile_image;
  std::optional<std::string> description;
  std::optional<std::string> website;
  std::optional<std::string> location;
};
inline constexpr auto user_profiles = relx::t<UserProfiles>;

// clang-format on

}  // namespace test_tables

// Test utilities
namespace test_utils {

// Helper to print SQL and parameters for a query
template <typename Query>
void print_query_details(const Query& query, const std::string& test_name) {
  std::cout << "\n=== " << test_name << " ===" << std::endl;
  std::cout << "SQL: " << query.to_sql() << std::endl;

  auto params = query.bind_params();
  std::cout << "Params (" << params.size() << "): ";
  for (size_t i = 0; i < params.size(); ++i) {
    std::cout << "[" << i << "]=" << params[i] << " ";
  }
  std::cout << std::endl << std::endl;
}

}  // namespace test_utils
