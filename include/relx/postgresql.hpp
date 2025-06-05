#pragma once

#include "connection/postgresql_async_connection.hpp"
#include "connection/postgresql_connection.hpp"
#include "utils/error_handling.hpp"

/**
 * @brief relx PostgreSQL database connections
 *
 * This file includes PostgreSQL-specific connection classes for both synchronous
 * and asynchronous database operations.
 *
 * Example usage:
 *
 * ```cpp
 * #include <relx/schema.hpp>
 * #include <relx/query.hpp>
 * #include <relx/postgresql.hpp>
 *
 * // Define a table
 * struct Users {
 *     static constexpr auto table_name = "users";
 *     relx::column<Users, "id", int> id;
 *     relx::column<Users, "name", std::string> name;
 *     relx::column<Users, "email", std::string> email;
 *
 *     relx::primary_key<&Users::id> pk;
 * };
 *
 * // Create connection
 * relx::PostgreSQLConnectionParams params{
 *     .host = "localhost",
 *     .port = 5432,
 *     .dbname = "example",
 *     .user = "postgres",
 *     .password = "postgres"
 * };
 *
 * relx::PostgreSQLConnection conn(params);
 *
 * // Connect
 * auto connect_result = conn.connect();
 * if (!connect_result) {
 *     std::println("Connection error: {}", connect_result.error().message);
 *     return 1;
 * }
 *
 * // Create a table
 * Users users;
 * auto create_table_query = relx::create_table(users);
 * conn.execute(create_table_query);
 *
 * // Insert with RETURNING clause (PostgreSQL specific)
 * auto insert = relx::insert_into(users)
 *     .values(
 *         relx::set(users.name, "John Doe"),
 *         relx::set(users.email, "john@example.com")
 *     )
 *     .returning(users.id);  // PostgreSQL supports RETURNING clause
 *
 * auto insert_result = conn.execute(insert);
 * if (insert_result) {
 *     int id = insert_result->at(0).get<int>("id").value_or(0);
 *     std::println("Inserted user with ID: {}", id);
 * }
 *
 * // Async connection example
 * relx::PostgreSQLAsyncConnection async_conn(params);
 * auto async_result = async_conn.connect();
 *
 * if (async_result) {
 *     // Execute query asynchronously
 *     auto query = relx::select(users.id, users.name)
 *         .from(users)
 *         .where(users.id > 0);
 *
 *     async_conn.execute(query, [](auto result) {
 *         if (result) {
 *             for (const auto& row : *result) {
 *                 int id = row.get<int>("id").value_or(0);
 *                 std::string name = row.get<std::string>("name").value_or("");
 *                 std::println("User {}: {}", id, name);
 *             }
 *         }
 *     });
 * }
 * ```
 */

namespace relx {
// Convenient import from the connection namespace
using connection::PostgreSQLAsyncConnection;
using connection::PostgreSQLConnection;
}  // namespace relx