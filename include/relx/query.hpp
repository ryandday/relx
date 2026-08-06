#pragma once

// uuid.hpp first: its val()/Value overloads must be declared before the query
// machinery that makes unqualified val(...) calls (boost types get no ADL here)
#include "query/arithmetic.hpp"
#include "query/column_expression.hpp"
#include "query/condition.hpp"
#include "query/core.hpp"
#include "query/date.hpp"
#include "query/delete.hpp"
#include "query/function.hpp"
#include "query/insert.hpp"
#include "query/literals.hpp"
#include "query/operators.hpp"
#include "query/row_type.hpp"
#include "query/schema_adapter.hpp"
#include "query/select.hpp"
#include "query/static_shape.hpp"
#include "query/static_sql.hpp"
#include "query/update.hpp"
#include "query/uuid.hpp"
#include "query/value.hpp"

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
 * struct [[=relx::table("users")]] Users {
 *     [[=relx::ann::pk]] int id;
 *     std::string name;
 *     std::string email;
 *     int age;
 * };
 *
 * // Define the table object once, next to the struct
 * inline constexpr auto u = relx::t<Users>;
 *
 * // Option 1: Simple select query with modern syntax
 * auto query1 = relx::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(u.age > 18);
 *
 * // Option 2: Using explicit functions for clarity
 * auto query2 = relx::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(relx::to_expr(u.age) > relx::val(18));
 *
 * // Option 3: Using SQL literal suffix
 * auto query3 = relx::select(u.id, u.name, u.email)
 *   .from(u)
 *   .where(u.age > 18_sql);
 *
 * // Get the SQL and parameters
 * std::string sql = query1.to_sql();  // SELECT users.id, users.name, users.email FROM users WHERE
 * (users.age > ?) auto params = query1.bind_params();  // ["18"]
 *
 * // Complex queries with joins
 * struct [[=relx::table("posts")]] Posts {
 *     [[=relx::ann::pk]] int id;
 *     [[=relx::ann::fk<^^Users::id>]] int user_id;
 *     std::string title;
 * };
 *
 * inline constexpr auto p = relx::t<Posts>;
 *
 * // Join query
 * auto join_query = relx::select(u.name, p.title)
 *     .from(u)
 *     .join(p, relx::on(u.id == p.user_id))
 *     .where(u.age > 21)
 *     .order_by(relx::desc(p.title));
 *
 * // Aggregation functions
 * auto agg_query = relx::select_expr(
 *     relx::count_all().as("user_count"),
 *     relx::avg(u.age).as("average_age")
 * )
 * .from(u)
 * .where(u.age > 21);
 *
 * // Update query
 * auto update_query = relx::update(u)
 *     .set(u.name, "John Smith")
 *     .set(u.email, "john.smith@example.com")
 *     .where(u.id == 1);
 *
 * // Delete query
 * auto delete_query = relx::delete_from(u)
 *     .where(u.age < 18);
 *
 * // Insert query
 * auto insert_query = relx::insert_into(u)
 *     .columns(u.name, u.email, u.age)
 *     .values(relx::val("Alice"), relx::val("alice@example.com"), relx::val(25));
 * ```
 */

namespace relx {

// Convenient imports from the query namespace
using query::as;
using query::asc;
using query::avg;
using query::case_;
using query::coalesce;
using query::count;
using query::count_all;
using query::count_distinct;
using query::delete_from;
using query::desc;
using query::distinct;
using query::exists;
using query::has_engaged_fields;
using query::in;
using query::in_any;
using query::insert_into;
using query::like;
using query::max;
using query::min;
using query::not_exists;
using query::on;
using query::select;
using query::select_all;
using query::select_expr;
using query::sum;
using query::update;
using query::val;

// Date/time functions
using query::abs;
using query::age_in_years;
using query::current_date;
using query::current_time;
using query::current_timestamp;
using query::date_add;
using query::date_diff;
using query::date_sub;
using query::date_trunc;
using query::day;
using query::day_of_week;
using query::day_of_year;
using query::days_since;
using query::days_until;
using query::extract;
using query::hour;
using query::interval;
using query::minute;
using query::month;
using query::now;
using query::second;
using query::start_of_day;
using query::start_of_month;
using query::start_of_year;
using query::year;

// User-defined literals
using namespace query::literals;  // Enables 42_sql, "string"_sql, etc.

// ===== INTERNAL/ADVANCED API =====
// These are implementation details that power users might need
// Most users should not use these directly
namespace detail {
using query::to_expr;
using query::to_table;
}  // namespace detail

}  // namespace relx
