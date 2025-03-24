#pragma once

#include "connection/connection.hpp"
#include "connection/sqlite_connection.hpp"

/**
 * @brief SQLlib database connection
 * 
 * This is the main include file for SQLlib database connection functionality.
 * It provides abstract classes and concrete implementations for connecting to databases.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <sqllib/schema.hpp>
 * #include <sqllib/query.hpp>
 * #include <sqllib/connection.hpp>
 * #include <sqllib/results.hpp>
 * 
 * // Define a table
 * struct Users {
 *     static constexpr auto table_name = "users";
 *     sqllib::schema::column<"id", int> id;
 *     sqllib::schema::column<"name", std::string> name;
 *     sqllib::schema::column<"email", std::string> email;
 *     sqllib::schema::column<"age", int> age;
 * };
 * 
 * // Create a SQLite connection
 * auto conn = sqllib::connection::SQLiteConnection("my_database.db");
 * 
 * // Connect to the database
 * auto connect_result = conn.connect();
 * if (!connect_result) {
 *     std::cerr << "Connection error: " << connect_result.error().message << std::endl;
 *     return 1;
 * }
 * 
 * // Create table instance
 * Users u;
 * 
 * // Create a simple select query
 * auto query = sqllib::query::select(u.id, u.name, u.email)
 *     .from(u)
 *     .where(sqllib::query::to_expr(u.age) > sqllib::query::val(18));
 * 
 * // Execute the query
 * auto result = conn.execute_raw(query);
 * if (!result) {
 *     std::cerr << "Query error: " << result.error().message << std::endl;
 *     return 1;
 * }
 * 
 * // Process the results
 * for (const auto& row : *result) {
 *     int id = *(row.get<int>(0));
 *     std::string name = *(row.get<std::string>(1));
 *     std::string email = *(row.get<std::string>(2));
 *     std::cout << id << ": " << name << " <" << email << ">" << std::endl;
 * }
 * 
 * // Disconnect
 * conn.disconnect();
 * ```
 */

namespace sqllib {

// Convenient imports from the connection namespace 
using connection::Connection;
using connection::ConnectionError;
using connection::ConnectionResult;
using connection::SQLiteConnection;

} // namespace sqllib 