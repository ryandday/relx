#pragma once

#include "schema/core.hpp"
#include "schema/column.hpp"
#include "schema/table.hpp"
#include "schema/primary_key.hpp"
#include "schema/foreign_key.hpp"
#include "schema/index.hpp"
#include "schema/fixed_string.hpp"
#include "schema/default_value.hpp"
#include "schema/check_constraint.hpp"
#include "schema/unique_constraint.hpp"

/**
 * @brief SQLlib - A type-safe SQL library
 * 
 * This is the main include file for the SQLlib library. It includes
 * all the necessary headers to define database schemas.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <sqllib/schema.hpp>
 * 
 * // Define a table
 * struct users : sqllib::schema::table<"users"> {
 *     // Define columns
 *     sqllib::schema::column<"id", int> id;
 *     sqllib::schema::column<"name", std::string> name;
 *     sqllib::schema::column<"email", std::string> email;
 *     
 *     // Nullable column using std::optional
 *     sqllib::schema::column<"bio", std::optional<std::string>> bio;
 *     
 *     // Column with default value - automatically deduces type
 *     sqllib::schema::column<"age", int, sqllib::schema::DefaultValue<18>> age;
 *     
 *     // Default value for float - automatically deduces type
 *     sqllib::schema::column<"score", double, sqllib::schema::DefaultValue<0.0>> score;
 *     
 *     // Default value for boolean - automatically deduces type
 *     sqllib::schema::column<"is_active", bool, sqllib::schema::DefaultValue<true>> is_active;
 *     
 *     // Default value for string - automatically deduces type
 *     sqllib::schema::column<"status", std::string, sqllib::schema::DefaultValue<"pending">> status;
 *     
 *     // SQL literal default value
 *     sqllib::schema::column<"created_at", std::string, sqllib::schema::DefaultValue<sqllib::schema::current_timestamp>> created_at;
 *     
 *     // Nullable with explicit NULL default
 *     sqllib::schema::column<"notes", std::optional<std::string>, sqllib::schema::null_default> notes;
 *     
 *     // Define a primary key
 *     sqllib::schema::primary_key<&users::id> pk;
 *     
 *     // Define a unique constraint
 *     sqllib::schema::unique_constraint<&users::email> unique_email;
 *     
 *     // Define a check constraint
 *     sqllib::schema::check_constraint<&users::age, ">= 18"> age_check;
 *     
 *     // Define a table-level check constraint
 *     sqllib::schema::table_check_constraint<"status IN ('pending', 'active', 'suspended')"> status_check;
 * };
 * ```
 * 
 * You can generate the SQL for creating the table:
 * ```cpp
 * users user_table;
 * std::string sql = sqllib::schema::create_table_sql(user_table);
 * ```
 * 
 * Result:
 * ```sql
 * CREATE TABLE users (
 *     id INTEGER NOT NULL,
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

namespace sqllib {

/**
 * Example usage with raw strings:
 * 
 * // Define tables with raw string literals
 * sqllib::schema::table<"users"> user_table;
 * 
 * // Define columns with raw string literals
 * sqllib::schema::column<"id", int> id_column;
 * sqllib::schema::column<"name", std::string> name_column;
 * 
 * // Define nullable columns with std::optional
 * sqllib::schema::column<"email", std::optional<std::string>> email_column;
 * 
 * // Define columns with strongly typed default values
 * sqllib::schema::column<"age", int, sqllib::schema::DefaultValue<18>> age_column;
 * sqllib::schema::column<"price", double, sqllib::schema::DefaultValue<0.0>> price_column;
 * sqllib::schema::column<"is_active", bool, sqllib::schema::DefaultValue<true>> is_active_column;
 * 
 * // Define columns with string default values
 * sqllib::schema::string_column_with_default<"status", "active"> status_column;
 * 
 * // Define columns with SQL literals as default
 * sqllib::schema::column<"created_at", std::string, sqllib::schema::DefaultValue<sqllib::schema::current_timestamp>> created_at_column;
 * 
 * // Define check constraints
 * sqllib::schema::check_constraint<&product::price, "> 0"> price_check;
 * 
 * // Define unique constraints
 * sqllib::schema::unique_constraint<&user::email> unique_email;
 * sqllib::schema::composite_unique_constraint<&user::first_name, &user::last_name> unique_name;
 */

} // namespace sqllib
