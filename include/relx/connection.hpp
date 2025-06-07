#pragma once

#include "connection/connection.hpp"
#include "connection/postgresql_async_connection.hpp"
#include "connection/postgresql_connection.hpp"
#include "connection/postgresql_connection_pool.hpp"
#include "connection/transaction_guard.hpp"
#include "utils/error_handling.hpp"
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
 *     relx::column<Users, "id", int> id;
 *     relx::column<Users, "name", std::string> name;
 *     relx::column<Users, "email", std::string> email;
 *     relx::column<Users, "age", int> age;
 *
 *     relx::primary_key<&Users::id> pk;
 * };
 *
 * // Define a DTO for results
 * struct UserDTO {
 *     int id;
 *     std::string name;
 *     std::string email;
 * };
 *
 * // Create a connection with connection parameters
 * relx::PostgreSQLConnectionParams params{
 *     .host = "localhost",
 *     .port = 5432,
 *     .dbname = "mydb",
 *     .user = "postgres",
 *     .password = "postgres"
 * };
 * auto conn = relx::PostgreSQLConnection(params);
 *
 * // Connect to the database
 * auto connect_result = conn.connect();
 * if (!connect_result) {
 *     std::println("Connection error: {}", connect_result.error().message);
 *     return 1;
 * }
 *
 * // Create table instance
 * Users u;
 *
 * // Create a simple select query
 * auto query = relx::select(u.id, u.name, u.email)
 *     .from(u)
 *     .where(u.age > 18);
 *
 * // Execute the query with automatic DTO mapping
 * auto result = conn.execute<UserDTO>(query);
 * if (!result) {
 *     std::println("Query error: {}", result.error().message);
 *     return 1;
 * }
 *
 * // Process the results using the DTO
 * for (const auto& user : *result) {
 *     std::println("{}: {} <{}>", user.id, user.name, user.email);
 * }
 *
 * // Alternatively, use structured bindings
 * auto raw_result = conn.execute(query);
 * if (raw_result) {
 *     for (const auto& [id, name, email] : raw_result->as<int, std::string, std::string>()) {
 *         std::println("{}: {} <{}>", id, name, email);
 *     }
 * }
 *
 * // Disconnect
 * conn.disconnect();
 * ```
 */

// Re-export connection components to the top-level namespace
namespace relx {
// Connection types
using connection::Connection;
using connection::PostgreSQLAsyncConnection;
using connection::PostgreSQLConnection;
using connection::PostgreSQLConnectionParams;
using connection::PostgreSQLConnectionPool;
using connection::PostgreSQLConnectionPoolConfig;
using connection::TransactionGuard;

// Error types
using connection::ConnectionError;
using connection::ConnectionPoolError;

// Result types
using connection::ConnectionPoolResult;
using connection::ConnectionResult;
using pgsql_async_wrapper::PgResult;
using query::QueryResult;
using result::ResultProcessingResult;
}  // namespace relx