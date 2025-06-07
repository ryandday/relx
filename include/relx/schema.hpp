#pragma once

#include "query/operators.hpp"
#include "schema/check_constraint.hpp"
#include "schema/chrono_traits.hpp"
#include "schema/column.hpp"
#include "schema/core.hpp"
#include "schema/fixed_string.hpp"
#include "schema/foreign_key.hpp"
#include "schema/index.hpp"
#include "schema/primary_key.hpp"
#include "schema/table.hpp"
#include "schema/unique_constraint.hpp"

/**
 * @brief relx - A type-safe SQL library
 *
 * This file includes all the necessary headers to define database schemas.
 *
 * Example usage:
 *
 * ```cpp
 * #include <relx/schema.hpp>
 *
 * // Define a table
 * struct Users {
 *     static constexpr auto table_name = "users";
 *
 *     // Define columns
 *     relx::column<Users, "id", int> id;
 *     relx::column<Users, "name", std::string> name;
 *     relx::column<Users, "email", std::string> email;
 *
 *     // Nullable column using std::optional
 *     relx::column<Users, "bio", std::optional<std::string>> bio;
 *
 *     // Column with default value
 *     relx::column<Users, "age", int, relx::default_value<18>> age;
 *
 *     // Default value for float
 *     relx::column<Users, "score", double, relx::default_value<0.0>> score;
 *
 *     // Default value for boolean
 *     relx::column<Users, "is_active", bool, relx::default_value<true>> is_active;
 *
 *     // Default value for string
 *     relx::column<Users, "status", std::string, relx::string_default<"pending">> status;
 *
 *     // SQL literal default value
 *     relx::column<Users, "created_at", std::string, relx::string_default<"CURRENT_TIMESTAMP",
 * true>> created_at;
 *
 *     // Nullable with explicit NULL default
 *     relx::column<Users, "notes", std::optional<std::string>, relx::null_default> notes;
 *
 *     // Define a primary key
 *     relx::primary_key<&Users::id> pk;
 *
 *     // Define a unique constraint
 *     relx::unique_constraint<&Users::email> unique_email;
 *
 *     // Define a check constraint on a column
 *     relx::table_check_constraint<&Users::age, ">= 18"> age_check;
 *
 *     // Define a table-level check constraint
 *     relx::table_check_constraint<"status IN ('pending', 'active', 'suspended')"> status_check;
 * };
 *
 * // Define another table with foreign key relationship
 * struct Posts {
 *     static constexpr auto table_name = "posts";
 *
 *     relx::column<Posts, "id", int> id;
 *     relx::column<Posts, "title", std::string> title;
 *     relx::column<Posts, "content", std::string> content;
 *     relx::column<Posts, "user_id", int> user_id;
 *
 *     // Define a primary key
 *     relx::primary_key<&Posts::id> pk;
 *
 *     // Define a foreign key relationship to Users table
 *     relx::foreign_key<&Posts::user_id, &Users::id> user_fk;
 *
 *     // Define an index for faster lookups
 *     relx::index<&Posts::user_id> user_id_idx;
 * };
 *
 * // Generate the SQL for creating the tables
 * Users users;
 * Posts posts;
 *
 * std::string users_sql = relx::create_table(users);
 * std::string posts_sql = relx::create_table(posts);
 *
 * // Result for users:
 * // CREATE TABLE IF NOT EXISTS users (
 * //     id INTEGER NOT NULL,
 * //     name TEXT NOT NULL,
 * //     email TEXT NOT NULL,
 * //     bio TEXT,
 * //     age INTEGER NOT NULL DEFAULT 18,
 * //     score REAL NOT NULL DEFAULT 0.0,
 * //     is_active BOOLEAN NOT NULL DEFAULT true,
 * //     status TEXT NOT NULL DEFAULT 'pending',
 * //     created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
 * //     notes TEXT DEFAULT NULL,
 * //     PRIMARY KEY (id),
 * //     UNIQUE (email),
 * //     CHECK (age >= 18),
 * //     CHECK (status IN ('pending', 'active', 'suspended'))
 * // )
 *
 * // Result for posts:
 * // CREATE TABLE IF NOT EXISTS posts (
 * //     id INTEGER NOT NULL,
 * //     title TEXT NOT NULL,
 * //     content TEXT NOT NULL,
 * //     user_id INTEGER NOT NULL,
 * //     PRIMARY KEY (id),
 * //     FOREIGN KEY (user_id) REFERENCES users (id)
 * // )
 *
 * // CREATE INDEX posts_user_id_idx ON posts (user_id)
 * ```
 */

namespace relx {

/**
 * Example usage with raw strings:
 *
 *
 * // Define columns with raw string literals
 * relx::column<Table, "id", int> id_column;
 * relx::column<Table, "name", std::string> name_column;
 *
 * // Define nullable columns with std::optional
 * relx::column<Table, "email", std::optional<std::string>> email_column;
 *
 * // Define columns with default values
 * relx::column<Table, "age", int, relx::default_value<18>> age_column;
 * relx::column<Table, "price", double, relx::default_value<0.0>> price_column;
 * relx::column<Table, "is_active", bool, relx::default_value<true>> is_active_column;
 *
 * // Define columns with string default values
 * relx::column<Table, "status", std::string, relx::string_default<"active">> status_column;
 *
 * // Define columns with SQL literals as default
 * relx::column<Table, "created_at", std::string, relx::string_default<"CURRENT_TIMESTAMP", true>>
 * created_at_column;
 *
 * // Define primary key
 * relx::primary_key<&Table::id> pk;
 *
 * // Define check constraints
 * relx::table_check_constraint<&Table::price, "> 0"> price_check;
 *
 * // Define unique constraints
 * relx::unique_constraint<&Table::email> unique_email;
 * relx::composite_unique_constraint<&Table::first_name, &Table::last_name> unique_name;
 */

// Re-export all schema types in the relx namespace
// These types are lowercase to look nicer
using schema::autoincrement;
using schema::column;
using schema::composite_unique_constraint;
using schema::create_table;
using schema::default_value;
using schema::drop_table;
using schema::fixed_string;
using schema::foreign_key;
using schema::identity;
using schema::index;
using schema::null_default;
using schema::primary_key;
using schema::string_default;
using schema::table_check_constraint;
using schema::table_primary_key;
using schema::unique;
using schema::unique_constraint;

}  // namespace relx
