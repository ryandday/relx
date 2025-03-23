#pragma once

#include "result.hpp"

/**
 * @brief SQLlib result processing
 * 
 * This is the main include file for SQLlib result processing functionality.
 * It provides a way to parse and process query results in a type-safe manner.
 * 
 * Example usage:
 * 
 * ```cpp
 * #include <sqllib/schema.hpp>
 * #include <sqllib/query.hpp>
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
 * // Create table instance
 * Users u;
 * 
 * // Create a simple select query
 * auto query = sqllib::query::select(u.id, u.name, u.email)
 *     .from(u)
 *     .where(sqllib::query::to_expr(u.age) > sqllib::query::val(18));
 * 
 * // Generate SQL and execute it (hypothetical)
 * std::string sql = query.to_sql();
 * std::string raw_results = execute_sql(sql, query.bind_params());
 * 
 * // Parse the results
 * auto result = sqllib::result::parse(query, raw_results);
 * 
 * // Process the results using different approaches
 * 
 * // 1. Simple iteration with indexed access
 * for (const auto& row : *result) {
 *     int id = *(row.get<int>(0));
 *     std::string name = *(row.get<std::string>(1));
 *     std::string email = *(row.get<std::string>(2));
 *     std::cout << id << ": " << name << " <" << email << ">" << std::endl;
 * }
 * 
 * // 2. Using column names
 * for (const auto& row : *result) {
 *     int id = *(row.get<int>("id"));
 *     std::string name = *(row.get<std::string>("name"));
 *     std::string email = *(row.get<std::string>("email"));
 *     std::cout << id << ": " << name << " <" << email << ">" << std::endl;
 * }
 * 
 * // 3. Using column objects
 * for (const auto& row : *result) {
 *     int id = *(row.get<int>(u.id));
 *     std::string name = *(row.get<std::string>(u.name));
 *     std::string email = *(row.get<std::string>(u.email));
 *     std::cout << id << ": " << name << " <" << email << ">" << std::endl;
 * }
 * 
 * // 4. Using member pointers
 * for (const auto& row : *result) {
 *     int id = *(row.get<&Users::id>());
 *     std::string name = *(row.get<&Users::name>());
 *     std::string email = *(row.get<&Users::email>());
 *     std::cout << id << ": " << name << " <" << email << ">" << std::endl;
 * }
 * 
 * // 5. Using structured binding (C++17)
 * for (const auto& [id, name, email] : result->as<int, std::string, std::string>()) {
 *     std::cout << id << ": " << name << " <" << email << ">" << std::endl;
 * }
 * 
 * // 6. Transforming to custom objects
 * struct UserData {
 *     int id;
 *     std::string name;
 *     std::string email;
 * };
 * 
 * auto users = result->transform<UserData>([](const auto& row) -> sqllib::result::ResultProcessingResult<UserData> {
 *     auto id = row.get<int>("id");
 *     auto name = row.get<std::string>("name");
 *     auto email = row.get<std::string>("email");
 *     
 *     if (!id || !name || !email) {
 *         return std::unexpected(sqllib::result::ResultError{"Error transforming row"});
 *     }
 *     
 *     return UserData{*id, *name, *email};
 * });
 * ```
 */

namespace sqllib {

// Convenient imports from the result namespace
using result::ResultError;
using result::ResultProcessingResult;
using result::Cell;
using result::Row;
using result::ResultSet;
using result::parse;

} // namespace sqllib 