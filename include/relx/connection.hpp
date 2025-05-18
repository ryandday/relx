#pragma once

#include "connection/connection.hpp"
#include "connection/postgresql_connection.hpp"
#include "connection/transaction_guard.hpp"
#include "connection/postgresql_connection_pool.hpp"
/**
 * @brief relx database connection
 * 
 * This is the main include file for relx database connection functionality.
 * It provides abstract classes and concrete implementations for connecting to databases.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <relx/schema.hpp>
 * #include <relx/query.hpp>
 * #include <relx/connection.hpp>
 * #include <relx/results.hpp>
 * 
 * // Define a table
 * struct Users {
 *     static constexpr auto table_name = "users";
 *     relx::schema::column<"id", int> id;
 *     relx::schema::column<"name", std::string> name;
 *     relx::schema::column<"email", std::string> email;
 *     relx::schema::column<"age", int> age;
 * };
 * 
 * // Create a SQLite connection
 * auto conn = relx::connection::SQLiteConnection("my_database.db");
 * 
 * // Connect to the database
 * auto connect_result = conn.connect();
 * if (!connect_result) {
 *     std::print("Connection error: {}", connect_result.error().message);
 *     return 1;
 * }
 * 
 * // Create table instance
 * Users u;
 * 
 * // Create a simple select query
 * auto query = relx::query::select(u.id, u.name, u.email)
 *     .from(u)
 *     .where(relx::query::to_expr(u.age) > relx::query::val(18));
 * 
 * // Execute the query
 * auto result = conn.execute_raw(query);
 * if (!result) {
 *     std::print("Query error: {}", result.error().message);
 *     return 1;
 * }
 * 
 * // Process the results
 * for (const auto& row : *result) {
 *     int id = *(row.get<int>(0));
 *     std::string name = *(row.get<std::string>(1));
 *     std::string email = *(row.get<std::string>(2));
 *     std::println("{}: {} <{}>", id, name, email);
 * }
 * 
 * // Disconnect
 * conn.disconnect();
 * ```
 */

namespace relx {

// Convenient imports from the connection namespace 
using connection::Connection;
using connection::ConnectionError;
using connection::ConnectionResult;
using connection::PostgreSQLConnection;
using connection::PostgreSQLConnectionPool;
using connection::PostgreSQLConnectionPoolConfig;
using connection::ConnectionPoolResult;
using connection::ConnectionPoolError;


} // namespace relx 