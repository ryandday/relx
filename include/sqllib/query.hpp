#pragma once

#include "query/core.hpp"
#include "query/column_expression.hpp"
#include "query/condition.hpp"
#include "query/value.hpp"
#include "query/select.hpp"
#include "query/update.hpp"
#include "query/delete.hpp"
#include "query/function.hpp"
#include "query/schema_adapter.hpp"
#include "query/helpers.hpp"

/**
 * @brief SQLlib query builder
 * 
 * This is the main include file for SQLlib query building functionality.
 * It provides a fluent interface for constructing type-safe SQL queries.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <sqllib/schema.hpp>
 * #include <sqllib/query.hpp>
 * 
 * // Define a table
 * struct Users {
 *     static constexpr auto table_name = "users";
 *     sqllib::schema::column<"id", int> id;
 *     sqllib::schema::column<"name", std::string> name;
 *     sqllib::schema::column<"email", std::string> email;
 *     sqllib::schema::column<"age", int> age;
 *     
 *     // ... other columns, constraints, etc.
 * };
 * 
 * // Create table instance
 * Users u;
 * 
 * // Create a simple select query - Option 1: Using helper functions
 * auto query1 = sqllib::query::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(sqllib::query::to_expr(u.age) > sqllib::query::val(18));
 *
 * // Option 2: Using explicit adapters
 * auto query2 = sqllib::query::select(
 *     sqllib::query::to_expr(u.id),
 *     sqllib::query::to_expr(u.name),
 *     sqllib::query::to_expr(u.email)
 * )
 * .from(sqllib::query::to_table(u))
 * .where(sqllib::query::to_expr(u.age) > sqllib::query::val(18));
 * 
 * // Get the SQL and parameters
 * std::string sql = query1.to_sql();                   // SELECT id, name, email FROM users WHERE (age > ?)
 * std::vector<std::string> params = query1.bind_params(); // ["18"]
 * 
 * // Aggregate functions
 * auto agg_query = sqllib::query::select_expr(
 *     sqllib::query::as(sqllib::query::count_all(), "user_count"),
 *     sqllib::query::as(sqllib::query::avg(sqllib::query::to_expr(u.age)), "average_age")
 * )
 * .from(u)
 * .where(sqllib::query::to_expr(u.age) > sqllib::query::val(21));
 * ```
 */

namespace sqllib {
namespace query {

// Convenient imports from the query namespace
using query::select;
using query::select_expr;
using query::update;
using query::delete_from;
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

} // namespace query
} // namespace sqllib 