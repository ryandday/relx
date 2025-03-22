#pragma once

#include "schema/core.hpp"
#include "schema/column.hpp"
#include "schema/table.hpp"
#include "schema/primary_key.hpp"
#include "schema/foreign_key.hpp"
#include "schema/index.hpp"
#include "schema/fixed_string.hpp"

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
 */

} // namespace sqllib
