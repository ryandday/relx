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
 * // Option 3: Using new concise API (with operator overloads)
 * auto query3 = sqllib::query::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(u.age > 18);
 *
 * // Option 4: Using new concise API with shorthand helper functions
 * using namespace sqllib::query;
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

namespace sqllib {

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

// New concise helpers
using query::v;      // shorthand for val
using query::e;      // shorthand for to_expr
using query::a;      // shorthand for as
using query::l;      // shorthand for like
using query::i;      // shorthand for in
using query::c;      // shorthand for count
using query::c_all;  // shorthand for count_all
using query::c_distinct; // shorthand for count_distinct
using query::s;      // shorthand for sum
using query::a_avg;  // shorthand for avg
using query::a_min;  // shorthand for min
using query::a_max;  // shorthand for max
using query::d;      // shorthand for distinct
using query::a_by;   // shorthand for asc (order by)
using query::d_by;   // shorthand for desc (order by)

// User-defined literals
using namespace query::literals; // Enables 42_sql, "string"_sql, etc.

} // namespace sqllib