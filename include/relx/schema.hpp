#pragma once

#include "query/operators.hpp"
#include "refl_types.hpp"
#include "schema/annotated_table.hpp"
#include "schema/chrono_traits.hpp"
#include "schema/column.hpp"
#include "schema/core.hpp"
#include "schema/fixed_string.hpp"
#include "schema/table.hpp"
#include "schema/uuid_traits.hpp"

/**
 * @brief relx - A type-safe SQL library
 *
 * This file includes all the necessary headers to define database schemas.
 *
 * Tables are plain aggregates annotated with relx annotations; the same struct
 * doubles as the DTO for query results:
 *
 * ```cpp
 * #include <relx/schema.hpp>
 *
 * struct [[=relx::table("users")]] Users {
 *   [[=relx::ann::pk]]     int id;
 *                          std::string name;
 *   [[=relx::ann::unique]] std::string email;
 *   [[=relx::default_value<18>{}]] int age;
 *   std::optional<std::string> bio;  // nullable
 * };
 *
 * inline constexpr auto users = relx::t<Users>;  // define next to the struct, once
 *
 * // DDL, built at compile time
 * constexpr auto ddl = relx::create_table_sql<Users>().if_not_exists().to_sql();
 * // CREATE TABLE IF NOT EXISTS users (
 * //     id INTEGER NOT NULL PRIMARY KEY,
 * //     name TEXT NOT NULL,
 * //     email TEXT NOT NULL UNIQUE,
 * //     age INTEGER NOT NULL DEFAULT 18,
 * //     bio TEXT
 * // );
 * ```
 *
 * Constraints that span columns are struct-level annotations (composite_pk,
 * composite_unique, composite_fk, index_on, check) — see
 * relx/schema/annotated_table.hpp for the full annotation vocabulary.
 */

namespace relx {

// Re-export all schema types in the relx namespace
// These types are lowercase to look nicer
using schema::autoincrement;
using schema::column;
using schema::create_enum_type_sql;
using schema::create_table;
using schema::default_sql;
using schema::default_value;
using schema::drop_enum_type_sql;
using schema::drop_table;
using schema::fixed_string;
using schema::identity;
using schema::native_enum;
using schema::null_default;
using schema::on_delete;
using schema::on_update;
using schema::primary_key;
using schema::references;
using schema::string_default;
using schema::unique;

}  // namespace relx
