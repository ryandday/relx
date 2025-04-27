#pragma once

#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

// Common test tables used across query tests
namespace test_tables {

// User table
struct users {
    static constexpr auto table_name = "users";
    relx::schema::column<users, "id", int> id;
    relx::schema::column<users, "name", std::string> name;
    relx::schema::column<users, "email", std::string> email;
    relx::schema::column<users, "age", int> age;
    relx::schema::column<users, "created_at", std::string> created_at;
    relx::schema::column<users, "is_active", bool> is_active;
    relx::schema::column<users, "bio", std::optional<std::string>> bio;
    relx::schema::column<users, "login_count", int> login_count;
    
    relx::schema::table_primary_key<&users::id> pk;
    relx::schema::unique_constraint<&users::email> unique_email;
};

// Post table with foreign key to users
struct posts {
    static constexpr auto table_name = "posts";
    relx::schema::column<posts, "id", int> id;
    relx::schema::column<posts, "user_id", int> user_id;
    relx::schema::column<posts, "title", std::string> title;
    relx::schema::column<posts, "content", std::string> content;
    relx::schema::column<posts, "views", int> views;
    relx::schema::column<posts, "created_at", std::string> created_at;
    relx::schema::column<posts, "is_published", bool> is_published;
    
    relx::schema::table_primary_key<&posts::id> pk;
    relx::schema::foreign_key<&posts::user_id, &users::id> user_fk;
};

// Comments table with foreign keys to posts and users
struct comments {
    static constexpr auto table_name = "comments";
    relx::schema::column<comments, "id", int> id;
    relx::schema::column<comments, "post_id", int> post_id;
    relx::schema::column<comments, "user_id", int> user_id;
    relx::schema::column<comments, "content", std::string> content;
    relx::schema::column<comments, "created_at", std::string> created_at;
    relx::schema::column<comments, "is_approved", bool> is_approved;
    
    relx::schema::table_primary_key<&comments::id> pk;
    relx::schema::foreign_key<&comments::post_id, &posts::id> post_fk;
    relx::schema::foreign_key<&comments::user_id, &users::id> user_fk;
};

// Tags table for a many-to-many relationship with posts
struct tags {
    static constexpr auto table_name = "tags";
    relx::schema::column<tags, "id", int> id;
    relx::schema::column<tags, "name", std::string> name;
    
    relx::schema::table_primary_key<&tags::id> pk;
    relx::schema::unique_constraint<&tags::name> unique_name;
};

// Junction table for posts <-> tags many-to-many relationship
struct post_tags {
    static constexpr auto table_name = "post_tags";
    relx::schema::column<post_tags, "post_id", int> post_id;
    relx::schema::column<post_tags, "tag_id", int> tag_id;
    
    relx::schema::composite_primary_key<&post_tags::post_id, &post_tags::tag_id> pk;
    relx::schema::foreign_key<&post_tags::post_id, &posts::id> post_fk;
    relx::schema::foreign_key<&post_tags::tag_id, &tags::id> tag_fk;
};

// User profile table with one-to-one relationship with users
struct user_profiles {
    static constexpr auto table_name = "user_profiles";
    relx::schema::column<user_profiles, "user_id", int> user_id;
    relx::schema::column<user_profiles, "profile_image", std::optional<std::string>> profile_image;
    relx::schema::column<user_profiles, "description", std::optional<std::string>> description;
    relx::schema::column<user_profiles, "website", std::optional<std::string>> website;
    relx::schema::column<user_profiles, "location", std::optional<std::string>> location;
    
    relx::schema::table_primary_key<&user_profiles::user_id> pk;
    relx::schema::foreign_key<&user_profiles::user_id, &users::id> user_fk;
};

} // namespace test_tables

// Test utilities
namespace test_utils {

// Helper to print SQL and parameters for a query
template <typename Query>
void print_query_details(const Query& query, const std::string& test_name) {
    std::cout << "\n=== " << test_name << " ===\n";
    std::cout << "SQL: " << query.to_sql() << std::endl;
    
    auto params = query.bind_params();
    std::cout << "Params (" << params.size() << "): ";
    for (size_t i = 0; i < params.size(); ++i) {
        std::cout << "[" << i << "]=" << params[i] << " ";
    }
    std::cout << std::endl;
}

} // namespace test_utils 