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
 *     // Define columns
 *     relx::schema::column<"id", int> id;
 *     relx::schema::column<"name", std::string> name;
 *     relx::schema::column<"email", std::string> email;
 *     
 *     // Nullable column using std::optional
 *     relx::schema::column<"bio", std::optional<std::string>> bio;
 *     
 *     // Column with default value - automatically deduces type
 *     relx::schema::column<"age", int, relx::schema::DefaultValue<18>> age;
 *     
 *     // Default value for float - automatically deduces type
 *     relx::schema::column<"score", double, relx::schema::DefaultValue<0.0>> score;
 *     
 *     // Default value for boolean - automatically deduces type
 *     relx::schema::column<"is_active", bool, relx::schema::DefaultValue<true>> is_active;
 *     
 *     // Default value for string - automatically deduces type
 *     relx::schema::column<"status", std::string, relx::schema::DefaultValue<"pending">> status;
 *     
 *     // SQL literal default value
 *     relx::schema::column<"created_at", std::string, relx::schema::DefaultValue<relx::schema::current_timestamp>> created_at;
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
 * ```
 * 
 * You can generate the SQL for creating the table:
 * ```cpp
 * users user_table;
 * std::string sql = relx::schema::create_table_sql(user_table);
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
 * // Define columns with strongly typed default values
 * relx::schema::column<"age", int, relx::schema::DefaultValue<18>> age_column;
 * relx::schema::column<"price", double, relx::schema::DefaultValue<0.0>> price_column;
 * relx::schema::column<"is_active", bool, relx::schema::DefaultValue<true>> is_active_column;
 * 
 * // Define columns with string default values
 * relx::schema::string_column_with_default<"status", "active"> status_column;
 * 
 * // Define columns with SQL literals as default
 * relx::schema::column<"created_at", std::string, relx::schema::DefaultValue<relx::schema::current_timestamp>> created_at_column;
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
using schema::DefaultValue;
using schema::null_default;
using schema::primary_key;
using schema::unique_constraint;
using schema::composite_unique_constraint;
using schema::check_constraint;
using schema::table_check_constraint;
using schema::foreign_key;
using schema::index;
using schema::create_table;
using schema::drop_table;

} // namespace relx
