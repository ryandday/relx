#pragma once

#include "query/core.hpp"
#include "query/column_expression.hpp"
#include "query/condition.hpp"
#include "query/value.hpp"
#include "query/select.hpp"
#include "query/update.hpp"
#include "query/delete.hpp"
#include "query/insert.hpp"
#include "query/function.hpp"
#include "query/schema_adapter.hpp"
#include "query/helpers.hpp"
#include "query/literals.hpp"
#include "query/operators.hpp"

/**
 * @brief relx query builder
 * 
 * This is the main include file for relx query building functionality.
 * It provides a fluent interface for constructing type-safe SQL queries.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <relx/schema.hpp>
 * #include <relx/query.hpp>
 * 
 * // Define a table
 * struct Users {
 *     static constexpr auto table_name = "users";
 *     relx::schema::column<"id", int> id;
 *     relx::schema::column<"name", std::string> name;
 *     relx::schema::column<"email", std::string> email;
 *     relx::schema::column<"age", int> age;
 *     
 *     // ... other columns, constraints, etc.
 * };
 * 
 * // Create table instance
 * Users u;
 * 
 * // Create a simple select query - Option 1: Using helper functions
 * auto query1 = relx::query::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(relx::query::to_expr(u.age) > relx::query::val(18));
 *
 * // Option 2: Using explicit adapters
 * auto query2 = relx::query::select(
 *     relx::query::to_expr(u.id),
 *     relx::query::to_expr(u.name),
 *     relx::query::to_expr(u.email)
 * )
 * .from(relx::query::to_table(u))
 * .where(relx::query::to_expr(u.age) > relx::query::val(18));
 * 
 * // Option 3: Using new concise API (with operator overloads)
 * auto query3 = relx::query::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(u.age > 18);
 *
 * // Option 4: Using new concise API with shorthand helper functions
 * using namespace relx::query;
 * auto query4 = select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(e(u.age) > v(18));
 *
 * // Option 5: Using SQL literal suffix
 * auto query5 = select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(u.age > 18_sql);
 * 
 * // Get the SQL and parameters
 * std::string sql = query1.to_sql();                   // SELECT id, name, email FROM users WHERE (age > ?)
 * std::vector<std::string> params = query1.bind_params(); // ["18"]
 * 
 * // Aggregate functions with concise syntax
 * auto agg_query = select_expr(
 *     a(c_all(), "user_count"),
 *     a(avg(e(u.age)), "average_age")
 * )
 * .from(u)
 * .where(u.age > 21);
 * ```
 */

namespace relx {

// Convenient imports from the query namespace
using query::select;
using query::select_expr;
using query::update;
using query::delete_from;
using query::insert_into;
using query::val;
using query::on;
using query::as;
using query::count;
using query::count_all;
using query::count_distinct;
using query::sum;
using query::avg;
using query::min;
using query::max;
using query::distinct;
using query::coalesce;
using query::case_;
using query::asc;
using query::desc;
using query::like;
using query::in;
using query::to_expr;
using query::to_table;

// User-defined literals
using namespace query::literals; // Enables 42_sql, "string"_sql, etc.

} // namespace relx
