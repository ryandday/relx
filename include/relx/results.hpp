#pragma once

#include "results/result.hpp"

/**
 * @brief relx result processing
 *
 * This is the main include file for relx result processing functionality.
 * It provides a way to parse and process query results in a type-safe manner.
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
 * // Create connection
 * auto conn = relx::PostgreSQLConnection(params);
 * auto connect_result = conn.connect();
 * if (!connect_result) {
 *     std::println("Connection error: {}", connect_result.error().message);
 *     return 1;
 * }
 *
 * // Create a DTO that matches query fields
 * struct UserDTO {
 *     int id;
 *     std::string name;
 *     std::string email;
 * };
 *
 * // Create table instance
 * Users u;
 *
 * // Create a simple select query
 * auto query = relx::select(u.id, u.name, u.email)
 *     .from(u)
 *     .where(u.age > 18);
 *
 * // Execute with automatic DTO mapping (preferred approach)
 * auto dto_result = conn.execute<UserDTO>(query);
 * if (dto_result) {
 *     for (const auto& user : *dto_result) {
 *         std::println("{}: {} <{}>", user.id, user.name, user.email);
 *     }
 * }
 *
 * // Execute and process the raw results
 * auto result = conn.execute(query);
 * if (!result) {
 *     std::println("Query error: {}", result.error().message);
 *     return 1;
 * }
 *
 * // 1. Simple iteration with indexed access
 * for (const auto& row : *result) {
 *     auto id = row.get<int>(0);
 *     auto name = row.get<std::string>(1);
 *     auto email = row.get<std::string>(2);
 *
 *     if (id && name && email) {
 *         std::println("{}: {} <{}>", *id, *name, *email);
 *     }
 * }
 *
 * // 2. Using column names
 * for (const auto& row : *result) {
 *     auto id = row.get<int>("id");
 *     auto name = row.get<std::string>("name");
 *     auto email = row.get<std::string>("email");
 *
 *     if (id && name && email) {
 *         std::println("{}: {} <{}>", *id, *name, *email);
 *     }
 * }
 *
 * // 3. Using column objects
 * for (const auto& row : *result) {
 *     auto id = row.get<int>(u.id);
 *     auto name = row.get<std::string>(u.name);
 *     auto email = row.get<std::string>(u.email);
 *
 *     if (id && name && email) {
 *         std::println("{}: {} <{}>", *id, *name, *email);
 *     }
 * }
 *
 * // 4. Using structured binding (C++17)
 * for (const auto& [id, name, email] : result->as<int, std::string, std::string>()) {
 *     std::println("{}: {} <{}>", id, name, email);
 * }
 *
 * // 5. Transforming to custom objects
 * struct UserData {
 *     int id;
 *     std::string name;
 *     std::string email;
 * };
 *
 * auto users = result->transform<UserData>([](const auto& row) ->
 * relx::ResultProcessingResult<UserData> { auto id = row.get<int>("id"); auto name =
 * row.get<std::string>("name"); auto email = row.get<std::string>("email");
 *
 *     if (!id || !name || !email) {
 *         return std::unexpected(relx::ResultError{"Error transforming row"});
 *     }
 *
 *     return UserData{*id, *name, *email};
 * });
 *
 * // Handle nullable columns with std::optional
 * auto optional_query = relx::select(u.id, u.name, u.email)
 *     .from(u);
 *
 * auto optional_result = conn.execute(optional_query);
 * if (optional_result) {
 *     for (const auto& row : *optional_result) {
 *         auto email = row.get<std::optional<std::string>>("email");
 *
 *         if (email && *email) {
 *             // Email exists and is not null
 *             std::println("Email: {}", **email);
 *         } else if (email) {
 *             // Email exists but is null
 *             std::println("Email is NULL");
 *         }
 *     }
 * }
 * ```
 */

namespace relx {

// Convenient imports from the result namespace
using result::Cell;
using result::parse;
using result::ResultError;
using result::ResultProcessingResult;
using result::ResultSet;
using result::Row;

}  // namespace relx