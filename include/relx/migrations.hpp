#pragma once

#include "migrations/core.hpp"
#include "migrations/diff.hpp"
#include "migrations/constraint_operations.hpp"
#include "schema.hpp"

/**
 * @brief relx::migrations - Database Schema Migration Library
 *
 * This library provides automatic database migration generation by diffing table structures.
 * 
 * ## Core API (Most Users)
 * 
 * The main functions you'll need for 99% of use cases:
 * - `generate_migration(old_table, new_table, options)` - Diff two table versions
 * - `generate_create_table_migration(table)` - Create table migration  
 * - `generate_drop_table_migration(table)` - Drop table migration
 * 
 * Plus `Migration` (for .forward_sql()/.rollback_sql()) and `MigrationOptions` (for column mappings).
 *
 * ## Advanced API (Power Users)
 * 
 * Available in `relx::migrations::advanced::` namespace for:
 * - Building custom migrations
 * - Testing individual components  
 * - Introspecting migration operations
 * - Extending the migration system
 *
 * Example usage:
 *
 * ```cpp
 * #include <relx/migrations.hpp>
 * #include <relx/schema.hpp>
 *
 * // Define your old table version
 * struct UsersV1 {
 *     static constexpr auto table_name = "users";
 *     
 *     relx::column<UsersV1, "id", int, relx::primary_key> id;
 *     relx::column<UsersV1, "name", std::string> name;
 *     relx::column<UsersV1, "email", std::string> email;
 *     
 *     relx::unique_constraint<&UsersV1::email> unique_email;
 * };
 *
 * // Define your new table version
 * struct UsersV2 {
 *     static constexpr auto table_name = "users";
 *     
 *     relx::column<UsersV2, "id", int, relx::primary_key> id;
 *     relx::column<UsersV2, "name", std::string> name;
 *     relx::column<UsersV2, "email", std::string> email;
 *     relx::column<UsersV2, "age", std::optional<int>> age;  // New column
 *     relx::column<UsersV2, "created_at", std::string, 
 *                  relx::string_default<"CURRENT_TIMESTAMP", true>> created_at;  // New column
 *     
 *     relx::unique_constraint<&UsersV2::email> unique_email;
 * };
 *
 * int main() {
 *     UsersV1 old_users;
 *     UsersV2 new_users;
 *     
 *     // Generate migration from V1 to V2
 *     auto migration = relx::migrations::generate_migration(old_users, new_users);
 *     
 *     // Get forward migration SQL
 *     auto forward_sqls = migration.forward_sql();
 *     for (const auto& sql : forward_sqls) {
 *         std::println("Forward: {}", sql);
 *     }
 *     // Output:
 *     // Forward: ALTER TABLE users ADD COLUMN age INTEGER;
 *     // Forward: ALTER TABLE users ADD COLUMN created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP;
 *     
 *     // Get rollback migration SQL
 *     auto rollback_sqls = migration.rollback_sql();
 *     for (const auto& sql : rollback_sqls) {
 *         std::println("Rollback: {}", sql);
 *     }
 *     // Output:
 *     // Rollback: ALTER TABLE users DROP COLUMN created_at;
 *     // Rollback: ALTER TABLE users DROP COLUMN age;
 *     
 *     // Check if migration has operations
 *     if (!migration.empty()) {
 *         std::println("Migration '{}' has {} operations", migration.name(), migration.size());
 *     }
 *     
 *     return 0;
 * }
 * ```
 *
 * ## Column and Constraint Renaming
 *
 * You can specify mappings to handle column renames and avoid data loss:
 *
 * ```cpp
 * // Old table structure
 * struct EmployeesV1 {
 *     static constexpr auto table_name = "employees";
 *     relx::column<EmployeesV1, "first_name", std::string> first_name;
 *     relx::column<EmployeesV1, "email_addr", std::string> email_addr;
 * };
 *
 * // New table structure with renamed columns  
 * struct EmployeesV2 {
 *     static constexpr auto table_name = "employees";
 *     relx::column<EmployeesV2, "given_name", std::string> given_name;  // renamed
 *     relx::column<EmployeesV2, "email", std::string> email;            // renamed
 * };
 *
 * EmployeesV1 old_table;
 * EmployeesV2 new_table;
 *
 * // Without mappings - causes data loss (DROP + ADD)
 * auto migration_data_loss = relx::migrations::generate_migration(old_table, new_table);
 * // Generates: DROP first_name, DROP email_addr, ADD given_name, ADD email
 *
 * // With mappings - preserves data (RENAME)
 * relx::migrations::MigrationOptions options;
 * options.column_mappings = {
 *     {"first_name", "given_name"},
 *     {"email_addr", "email"}
 * };
 *
 * auto migration_safe = relx::migrations::generate_migration(old_table, new_table, options);
 * auto safe_sqls = migration_safe.forward_sql();
 * // Output:
 * // ALTER TABLE employees RENAME COLUMN first_name TO given_name;
 * // ALTER TABLE employees RENAME COLUMN email_addr TO email;
 *
 * auto safe_rollback = migration_safe.rollback_sql();
 * // Output (in reverse order):
 * // ALTER TABLE employees RENAME COLUMN email TO email_addr;
 * // ALTER TABLE employees RENAME COLUMN given_name TO first_name;
 * ```
 *
 * ## Column Rename + Type Change
 *
 * You can also handle cases where a column is both renamed and has its type changed:
 *
 * ```cpp
 * struct ProductsV1 {
 *     static constexpr auto table_name = "products";
 *     relx::column<ProductsV1, "price_cents", int> price_cents;  // int cents
 * };
 *
 * struct ProductsV2 {
 *     static constexpr auto table_name = "products";  
 *     relx::column<ProductsV2, "price_dollars", std::string> price_dollars;  // string dollars
 * };
 *
 * ProductsV1 old_products;
 * ProductsV2 new_products;
 *
 * relx::migrations::MigrationOptions options;
 * options.column_mappings = {{"price_cents", "price_dollars"}};
 * options.column_transformations = {
 *     {"price_cents", {"CAST(price_cents / 100.0 AS TEXT)", "CAST(REPLACE(price_dollars, '$', '') AS DECIMAL) * 100"}}
 * };
 *
 * auto migration = relx::migrations::generate_migration(old_products, new_products, options);
 * auto forward_sqls = migration.forward_sql();
 * // Output:
 * // ALTER TABLE products ADD COLUMN price_dollars TEXT NOT NULL;
 * // UPDATE products SET price_dollars = CAST(price_cents / 100.0 AS TEXT);
 * // ALTER TABLE products DROP COLUMN price_cents;
 *
 * auto rollback_sqls = migration.rollback_sql();
 * // Output (in reverse order):
 * // ALTER TABLE products ADD COLUMN price_cents INTEGER NOT NULL;
 * // UPDATE products SET price_cents = CAST(REPLACE(price_dollars, '$', '') AS DECIMAL) * 100;
 * // ALTER TABLE products DROP COLUMN price_dollars;
 * ```
 *
 * You can also generate migrations for creating or dropping entire tables:
 *
 * ```cpp
 * // Create a new table
 * struct NewTable {
 *     static constexpr auto table_name = "new_table";
 *     relx::column<NewTable, "id", int, relx::primary_key> id;
 *     relx::column<NewTable, "data", std::string> data;
 * };
 *
 * NewTable new_table;
 *
 * // Generate CREATE TABLE migration
 * auto create_migration = relx::migrations::generate_create_table_migration(new_table);
 * auto create_sqls = create_migration.forward_sql();
 * // Forward: CREATE TABLE new_table (id INTEGER NOT NULL, data TEXT NOT NULL, PRIMARY KEY (id));
 *
 * auto create_rollback_sqls = create_migration.rollback_sql();
 * // Rollback: DROP TABLE IF EXISTS new_table;
 *
 * // Generate DROP TABLE migration
 * auto drop_migration = relx::migrations::generate_drop_table_migration(new_table);
 * auto drop_sqls = drop_migration.forward_sql();
 * // Forward: DROP TABLE IF EXISTS new_table;
 *
 * auto drop_rollback_sqls = drop_migration.rollback_sql();
 * // Rollback: CREATE TABLE new_table (id INTEGER NOT NULL, data TEXT NOT NULL, PRIMARY KEY (id));
 * ```
 */

namespace relx {

// Re-export migration functionality in the relx namespace for easier access
namespace migrations {
    // Main migration functions
    using ::relx::migrations::generate_migration;
    using ::relx::migrations::generate_create_table_migration;
    using ::relx::migrations::generate_drop_table_migration;
    
    // Essential types for using the API
    using ::relx::migrations::Migration;
    using ::relx::migrations::MigrationOptions;
}

} // namespace relx 