#pragma once

#include "schema/core.hpp"
#include "schema/column.hpp"
#include "schema/table.hpp"
#include "schema/primary_key.hpp"
#include "schema/foreign_key.hpp"
#include "schema/index.hpp"
#include "schema/fixed_string.hpp"
#include "schema/check_constraint.hpp"
#include "schema/unique_constraint.hpp"
#include "query/operators.hpp"

/**
 * @brief relx - A type-safe SQL library
 * 
 * This is the main include file for the relx library. It includes
 * all the necessary headers to define database schemas.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <relx/schema.hpp>
 * 
 * // Define a table
 * struct users { 
 *     static constexpr auto table_name = "users";
 *     
 *     // Define columns
 *     relx::schema::column<"id", int> id;
 *     relx::schema::column<"name", std::string> name;
 *     relx::schema::column<"email", std::string> email;
 *     
 *     // Nullable column using std::optional
 *     relx::schema::column<"bio", std::optional<std::string>> bio;
 *     
 *     // Column with default value - uses default_value template
 *     relx::schema::column<"age", int, relx::schema::default_value<18>> age;
 *     
 *     // Default value for float
 *     relx::schema::column<"score", double, relx::schema::default_value<0.0>> score;
 *     
 *     // Default value for boolean
 *     relx::schema::column<"is_active", bool, relx::schema::default_value<true>> is_active;
 *     
 *     // Default value for string
 *     relx::schema::column<"status", std::string, relx::schema::string_default<"pending">> status;
 *     
 *     // SQL literal default value
 *     relx::schema::column<"created_at", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP", true>> created_at;
 *     
 *     // Nullable with explicit NULL default
 *     relx::schema::column<"notes", std::optional<std::string>, relx::schema::null_default> notes;
 *     
 *     // Define a primary key
 *     relx::schema::primary_key<&users::id> pk;
 *     
 *     // Define a unique constraint
 *     relx::schema::unique_constraint<&users::email> unique_email;
 *     
 *     // Define a check constraint
 *     relx::schema::check_constraint<&users::age, ">= 18"> age_check;
 *     
 *     // Define a table-level check constraint
 *     relx::schema::table_check_constraint<"status IN ('pending', 'active', 'suspended')"> status_check;
 * };
 * 
 * // Example with autoincrement primary key
 * struct users_with_autoincrement {
 *     static constexpr auto table_name = "users";
 *     
 *     // For PostgreSQL, you would use:
 *     // relx::schema::pg_serial<"id"> id;
 *     
 *     // Regular columns
 *     relx::schema::column<"name", std::string> name;
 *     relx::schema::column<"email", std::string> email;
 * };
 * ```
 * 
 * You can generate the SQL for creating the table:
 * ```cpp
 * users user_table;
 * std::string sql = relx::schema::create_table(user_table);
 * 
 * users_with_autoincrement auto_table;
 * std::string auto_sql = relx::schema::create_table(auto_table);
 * ```
 * 
 * Result for users:
 * ```sql
 * CREATE TABLE IF NOT EXISTS users (
 *     id INTEGER PRIMARY KEY,
 *     name TEXT NOT NULL,
 *     email TEXT NOT NULL,
 *     bio TEXT,
 *     age INTEGER NOT NULL DEFAULT 18,
 *     score REAL NOT NULL DEFAULT 0.0,
 *     is_active BOOLEAN NOT NULL DEFAULT true,
 *     status TEXT NOT NULL DEFAULT 'pending',
 *     created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
 *     notes TEXT DEFAULT NULL,
 *     PRIMARY KEY (id),
 *     UNIQUE (email),
 *     CHECK (age >= 18),
 *     CHECK (status IN ('pending', 'active', 'suspended'))
 * )
 * ```
 */

namespace relx {

/**
 * Example usage with raw strings:
 * 
 * 
 * // Define columns with raw string literals
 * relx::schema::column<"id", int> id_column;
 * relx::schema::column<"name", std::string> name_column;
 * 
 * // Define nullable columns with std::optional
 * relx::schema::column<"email", std::optional<std::string>> email_column;
 * 
 * // Define columns with default values
 * relx::schema::column<"age", int, relx::schema::default_value<18>> age_column;
 * relx::schema::column<"price", double, relx::schema::default_value<0.0>> price_column;
 * relx::schema::column<"is_active", bool, relx::schema::default_value<true>> is_active_column;
 * 
 * // Define columns with string default values
 * relx::schema::column<"status", std::string, relx::schema::string_default<"active">> status_column;
 * 
 * // Define columns with SQL literals as default
 * relx::schema::column<"created_at", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP", true>> created_at_column;
 * 
 * // Define autoincrement primary key columns
 * relx::schema::column<"id", int, relx::schema::autoincrement> sqlite_id_column;
 * relx::schema::column<"id", int, relx::schema::serial> postgres_id_column;
 * 
 * // Define check constraints
 * relx::schema::check_constraint<&product::price, "> 0"> price_check;
 * 
 * // Define unique constraints
 * relx::schema::unique_constraint<&user::email> unique_email;
 * relx::schema::composite_unique_constraint<&user::first_name, &user::last_name> unique_name;
 */

// Re-export all schema types in the relx namespace
using schema::FixedString;
using schema::column;
using schema::column_traits;
using schema::default_value;
using schema::string_default;
using schema::null_default;
using schema::table_primary_key;
using schema::unique_constraint;
using schema::composite_unique_constraint;
using schema::table_check_constraint;
using schema::foreign_key;
using schema::index;
using schema::create_table;
using schema::drop_table;

} // namespace relx
