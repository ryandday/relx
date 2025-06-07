# relx::migrations - Database Schema Migration System

## Overview

The `relx::migrations` system provides automatic database migration generation by comparing table structures. It can generate SQL DDL statements for creating, modifying, and dropping database tables and their components (columns, constraints, indexes).

### Key Features

- ✅ **Automatic Migration Generation**: Compare old and new table structures to generate migrations
- ✅ **Bidirectional Migrations**: Generate both forward and rollback SQL
- ✅ **Column Mapping Support**: Handle column renames without data loss
- ✅ **Type Transformations**: Convert data between different column types
- ✅ **Constraint Management**: Add, drop, and modify table constraints
- ✅ **Table Operations**: Create and drop entire tables
- ✅ **Type-Safe**: Leverages C++ templates for compile-time validation
- ✅ **CLI Tool Support**: Built-in command line interface utilities

## Quick Start

```cpp
#include <relx/migrations.hpp>
#include <relx/schema.hpp>

// Define your table versions
struct UsersV1 {
    static constexpr auto table_name = "users";
    
    relx::column<UsersV1, "id", int, relx::primary_key> id;
    relx::column<UsersV1, "name", std::string> name;
    relx::column<UsersV1, "email", std::string> email;
    
    relx::unique_constraint<&UsersV1::email> unique_email;
};

struct UsersV2 {
    static constexpr auto table_name = "users";
    
    relx::column<UsersV2, "id", int, relx::primary_key> id;
    relx::column<UsersV2, "name", std::string> name;
    relx::column<UsersV2, "email", std::string> email;
    relx::column<UsersV2, "age", std::optional<int>> age;  // New nullable column
    relx::column<UsersV2, "created_at", std::string, 
                 relx::string_default<"CURRENT_TIMESTAMP", true>> created_at;  // New column with default
    
    relx::unique_constraint<&UsersV2::email> unique_email;
};

int main() {
    UsersV1 old_users;
    UsersV2 new_users;
    
    // Generate migration
    auto migration = relx::migrations::generate_migration(old_users, new_users);
    
    // Get forward migration SQL
    auto forward_sqls = migration.forward_sql();
    for (const auto& sql : forward_sqls) {
        std::cout << "Forward: " << sql << std::endl;
    }
    // Output:
    // Forward: ALTER TABLE users ADD COLUMN age INTEGER;
    // Forward: ALTER TABLE users ADD COLUMN created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP;
    
    // Get rollback migration SQL  
    auto rollback_sqls = migration.rollback_sql();
    for (const auto& sql : rollback_sqls) {
        std::cout << "Rollback: " << sql << std::endl;
    }
    // Output:
    // Rollback: ALTER TABLE users DROP COLUMN created_at;
    // Rollback: ALTER TABLE users DROP COLUMN age;
    
    return 0;
}
```

## Core API Reference

### Main Functions

#### `generate_migration(old_table, new_table, options = {})`

Generates a migration by comparing two table structures.

```cpp
template <schema::TableConcept OldTable, schema::TableConcept NewTable>
Migration generate_migration(const OldTable& old_table, const NewTable& new_table, 
                           const MigrationOptions& options = {});
```

**Parameters:**
- `old_table`: The current table structure
- `new_table`: The target table structure  
- `options`: Optional configuration for column mappings and transformations

**Returns:** A `Migration` object containing the operations needed to transform the old table into the new table.

#### `generate_create_table_migration(table)`

Generates a migration to create a new table.

```cpp
template <schema::TableConcept Table>
Migration generate_create_table_migration(const Table& table);
```

#### `generate_drop_table_migration(table)`

Generates a migration to drop an existing table.

```cpp
template <schema::TableConcept Table>  
Migration generate_drop_table_migration(const Table& table);
```

### Migration Class

The `Migration` class contains a sequence of migration operations and provides methods to generate SQL.

```cpp
class Migration {
public:
    // Generate forward migration SQL statements
    std::vector<std::string> forward_sql() const;
    
    // Generate rollback migration SQL statements (in reverse order)
    std::vector<std::string> rollback_sql() const;
    
    // Get migration name
    const std::string& name() const;
    
    // Check if migration has any operations
    bool empty() const;
    
    // Get number of operations
    size_t size() const;
};
```

### Migration Options

Use `MigrationOptions` to configure advanced migration behavior:

```cpp
struct MigrationOptions {
    // Map old column names to new column names (for renames)
    std::unordered_map<std::string, std::string> column_mappings;
    
    // Map column names to forward/backward transformation expressions
    std::unordered_map<std::string, std::pair<std::string, std::string>> column_transformations;
};
```

## Migration Operation Types

The system supports the following migration operations:

| Operation | Description | Example |
|-----------|-------------|---------|
| `CREATE_TABLE` | Create a new table | `CREATE TABLE users (...);` |
| `DROP_TABLE` | Drop an existing table | `DROP TABLE IF EXISTS users;` |
| `ADD_COLUMN` | Add a new column | `ALTER TABLE users ADD COLUMN age INTEGER;` |
| `DROP_COLUMN` | Remove a column | `ALTER TABLE users DROP COLUMN age;` |
| `RENAME_COLUMN` | Rename a column | `ALTER TABLE users RENAME COLUMN old_name TO new_name;` |
| `MODIFY_COLUMN` | Change column type/properties | Generated as ADD + UPDATE + DROP sequence |
| `ADD_CONSTRAINT` | Add table constraint | `ALTER TABLE users ADD UNIQUE (email);` |
| `DROP_CONSTRAINT` | Remove table constraint | `ALTER TABLE users DROP CONSTRAINT constraint_name;` |
| `UPDATE_DATA` | Transform column data | `UPDATE users SET new_col = transform(old_col);` |

## Advanced Usage Patterns

### Column Renaming

Handle column renames without data loss using column mappings:

```cpp
// Old table structure
struct EmployeesV1 {
    static constexpr auto table_name = "employees";
    relx::column<EmployeesV1, "first_name", std::string> first_name;
    relx::column<EmployeesV1, "email_addr", std::string> email_addr;
};

// New table structure with renamed columns
struct EmployeesV2 {
    static constexpr auto table_name = "employees";
    relx::column<EmployeesV2, "given_name", std::string> given_name;  // renamed
    relx::column<EmployeesV2, "email", std::string> email;            // renamed
};

EmployeesV1 old_table;
EmployeesV2 new_table;

// ❌ WITHOUT mappings - causes data loss (DROP + ADD)
auto migration_data_loss = relx::migrations::generate_migration(old_table, new_table);
// Generates: DROP first_name, DROP email_addr, ADD given_name, ADD email

// ✅ WITH mappings - preserves data (RENAME)
relx::migrations::MigrationOptions options;
options.column_mappings = {
    {"first_name", "given_name"},
    {"email_addr", "email"}
};

auto migration_safe = relx::migrations::generate_migration(old_table, new_table, options);
auto safe_sqls = migration_safe.forward_sql();
// Output:
// ALTER TABLE employees RENAME COLUMN first_name TO given_name;
// ALTER TABLE employees RENAME COLUMN email_addr TO email;
```

### Column Type Changes with Data Transformation

Handle type changes while preserving and transforming data:

```cpp
struct ProductsV1 {
    static constexpr auto table_name = "products";
    relx::column<ProductsV1, "price_cents", int> price_cents;  // int cents
};

struct ProductsV2 {
    static constexpr auto table_name = "products";
    relx::column<ProductsV2, "price_dollars", std::string> price_dollars;  // string dollars
};

ProductsV1 old_products;
ProductsV2 new_products;

relx::migrations::MigrationOptions options;
options.column_mappings = {{"price_cents", "price_dollars"}};
options.column_transformations = {
    {"price_cents", {
        "CAST(price_cents / 100.0 AS TEXT) || ' USD'",  // forward: cents -> dollars string
        "CAST(REPLACE(price_dollars, ' USD', '') AS DECIMAL) * 100"  // backward: dollars string -> cents
    }}
};

auto migration = relx::migrations::generate_migration(old_products, new_products, options);
auto forward_sqls = migration.forward_sql();
// Output:
// ALTER TABLE products ADD COLUMN price_dollars TEXT NOT NULL;
// UPDATE products SET price_dollars = CAST(price_cents / 100.0 AS TEXT) || ' USD';
// ALTER TABLE products DROP COLUMN price_cents;

auto rollback_sqls = migration.rollback_sql();
// Output (in reverse order):
// ALTER TABLE products ADD COLUMN price_cents INTEGER NOT NULL;
// UPDATE products SET price_cents = CAST(REPLACE(price_dollars, ' USD', '') AS DECIMAL) * 100;
// ALTER TABLE products DROP COLUMN price_dollars;
```

### Constraint Management

The system automatically handles constraint changes:

```cpp
struct TableWithoutConstraints {
    static constexpr auto table_name = "users";
    relx::column<TableWithoutConstraints, "id", int, relx::primary_key> id;
    relx::column<TableWithoutConstraints, "email", std::string> email;
};

struct TableWithConstraints {
    static constexpr auto table_name = "users";
    relx::column<TableWithConstraints, "id", int, relx::primary_key> id;
    relx::column<TableWithConstraints, "email", std::string> email;
    
    relx::unique_constraint<&TableWithConstraints::email> unique_email;
};

TableWithoutConstraints old_table;
TableWithConstraints new_table;

auto migration = relx::migrations::generate_migration(old_table, new_table);
auto forward_sqls = migration.forward_sql();
// Output: ALTER TABLE users ADD UNIQUE (email);

auto rollback_sqls = migration.rollback_sql();  
// Output: ALTER TABLE users DROP CONSTRAINT users_unique_0;
```

### Table Creation and Deletion

Create migrations for entirely new or removed tables:

```cpp
struct NewTable {
    static constexpr auto table_name = "analytics";
    relx::column<NewTable, "id", int, relx::primary_key> id;
    relx::column<NewTable, "event", std::string> event;
    relx::column<NewTable, "timestamp", std::string> timestamp;
};

NewTable new_table;

// Create table migration
auto create_migration = relx::migrations::generate_create_table_migration(new_table);
auto create_sqls = create_migration.forward_sql();
// Forward: CREATE TABLE analytics (id INTEGER NOT NULL, event TEXT NOT NULL, timestamp TEXT NOT NULL, PRIMARY KEY (id));

auto create_rollback_sqls = create_migration.rollback_sql();
// Rollback: DROP TABLE IF EXISTS analytics;

// Drop table migration
auto drop_migration = relx::migrations::generate_drop_table_migration(new_table);
auto drop_sqls = drop_migration.forward_sql();
// Forward: DROP TABLE IF EXISTS analytics;

auto drop_rollback_sqls = drop_migration.rollback_sql();
// Rollback: CREATE TABLE analytics (...);
```

## Best Practices

### 1. Always Test Migrations

Build and run tests for every migration to ensure they work correctly:

```cpp
// In your test file
TEST(MigrationTest, TestUsersMigrationV1ToV2) {
    UsersV1 old_users;
    UsersV2 new_users;
    
    auto migration = relx::migrations::generate_migration(old_users, new_users);
    
    EXPECT_FALSE(migration.empty());
    EXPECT_EQ(migration.size(), 2);  // Should add 2 columns
    
    auto forward_sqls = migration.forward_sql();
    EXPECT_EQ(forward_sqls[0], "ALTER TABLE users ADD COLUMN age INTEGER;");
    EXPECT_EQ(forward_sqls[1], "ALTER TABLE users ADD COLUMN created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP;");
    
    auto rollback_sqls = migration.rollback_sql();
    EXPECT_EQ(rollback_sqls[0], "ALTER TABLE users DROP COLUMN created_at;");
    EXPECT_EQ(rollback_sqls[1], "ALTER TABLE users DROP COLUMN age;");
}
```

### 2. Use Column Mappings for Renames

Always use column mappings when renaming columns to avoid data loss:

```cpp
// ✅ Good - preserves data
relx::migrations::MigrationOptions options;
options.column_mappings = {{"old_name", "new_name"}};
auto safe_migration = relx::migrations::generate_migration(old_table, new_table, options);

// ❌ Bad - loses data
auto unsafe_migration = relx::migrations::generate_migration(old_table, new_table);
```

### 3. Handle Type Changes Carefully  

When changing column types, provide transformations to preserve data integrity:

```cpp
relx::migrations::MigrationOptions options;
options.column_mappings = {{"old_col", "new_col"}};
options.column_transformations = {
    {"old_col", {
        "CAST(old_col AS NEW_TYPE)",      // forward transformation
        "CAST(new_col AS OLD_TYPE)"       // backward transformation  
    }}
};
```

### 4. Version Your Table Structures

Use clear versioning for your table structures:

```cpp
// Good naming convention
struct UsersV1 { /* ... */ };
struct UsersV2 { /* ... */ };
struct UsersV3 { /* ... */ };

// Alternative naming
struct Users_2024_01 { /* ... */ };
struct Users_2024_02 { /* ... */ };
```

### 5. Review Generated SQL

Always review the generated SQL before applying to production:

```cpp
auto migration = relx::migrations::generate_migration(old_table, new_table);

std::cout << "=== Migration: " << migration.name() << " ===" << std::endl;
std::cout << "Operations: " << migration.size() << std::endl;

auto forward_sqls = migration.forward_sql();
std::cout << "\nForward Migration:" << std::endl;
for (size_t i = 0; i < forward_sqls.size(); ++i) {
    std::cout << (i + 1) << ". " << forward_sqls[i] << std::endl;
}

auto rollback_sqls = migration.rollback_sql();
std::cout << "\nRollback Migration:" << std::endl;
for (size_t i = 0; i < rollback_sqls.size(); ++i) {
    std::cout << (i + 1) << ". " << rollback_sqls[i] << std::endl;
}
```

### 6. Use Transactions

Wrap migration execution in transactions for atomicity:

```cpp
// Pseudo-code for migration execution
void apply_migration(const Migration& migration) {
    db.begin_transaction();
    try {
        auto sqls = migration.forward_sql();
        for (const auto& sql : sqls) {
            db.execute(sql);
        }
        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}
```

### 7. Keep Migration History

Store applied migrations to track schema evolution:

```cpp
struct MigrationRecord {
    std::string name;
    std::string applied_at;
    std::vector<std::string> forward_sql;
    std::vector<std::string> rollback_sql;
};

// Track what migrations have been applied
std::vector<MigrationRecord> applied_migrations;
```

## Common Patterns

### Incremental Schema Evolution

```cpp
// Define evolution path
UsersV1 users_v1;
UsersV2 users_v2;  
UsersV3 users_v3;

// Generate incremental migrations
auto migration_v1_to_v2 = relx::migrations::generate_migration(users_v1, users_v2);
auto migration_v2_to_v3 = relx::migrations::generate_migration(users_v2, users_v3);

// Apply in sequence
apply_migration(migration_v1_to_v2);
apply_migration(migration_v2_to_v3);
```

### Conditional Migrations

```cpp
auto migration = relx::migrations::generate_migration(old_table, new_table);

if (!migration.empty()) {
    std::cout << "Applying migration: " << migration.name() << std::endl;
    apply_migration(migration);
} else {
    std::cout << "No migration needed - tables are identical" << std::endl;
}
```

### Complex Transformations

```cpp
// For complex data transformations, you might need multiple steps
relx::migrations::MigrationOptions options;
options.column_mappings = {{"old_format", "new_format"}};
options.column_transformations = {
    {"old_format", {
        // Complex JSON transformation
        "JSON_OBJECT('data', old_format, 'version', 2, 'migrated_at', CURRENT_TIMESTAMP)",
        "JSON_EXTRACT(new_format, '$.data')"
    }}
};
```

## Troubleshooting

### Empty Migrations

If `generate_migration()` returns an empty migration when you expect changes:

1. **Check table names match**: Ensure both table structures have the same `table_name`
2. **Verify column definitions**: Make sure column types and constraints are properly defined
3. **Review boost::pfr compatibility**: Ensure your structs are aggregates (no constructors)

### Data Loss Warnings

The system will generate DROP + ADD operations instead of RENAME when:

1. Column mappings are not provided for renamed columns
2. Column types change without transformations
3. Constraints are removed and re-added

Always use `MigrationOptions` to preserve data during schema changes.

### Rollback Failures

Rollback migrations might fail if:

1. Forward transformations are not reversible
2. Data has been modified after migration
3. Constraints prevent rollback operations

Always test rollback scenarios in development environments.

## Performance Considerations

### Large Tables

For large tables, consider:

1. **Batch operations**: Break large UPDATEs into smaller batches
2. **Online migrations**: Use database-specific online migration features
3. **Staged rollouts**: Apply migrations during maintenance windows

### Index Management

The system handles constraint-based indexes but for explicit indexes:

1. Drop indexes before ALTER TABLE operations
2. Recreate indexes after schema changes
3. Consider index rebuild strategies for large tables

## Integration Examples

### CMake Integration

```cmake
# In your CMakeLists.txt
find_package(relx REQUIRED)

add_executable(migration_tool migration_tool.cpp)
target_link_libraries(migration_tool relx::migrations)
```

### CI/CD Pipeline

```bash
#!/bin/bash
# migration_check.sh

# Generate and review migrations
./migration_tool --generate --from=v1 --to=v2 --output=migration.sql

# Validate migration
./migration_tool --validate migration.sql

# Apply in test environment
./migration_tool --apply migration.sql --database=test

# Run tests
make test
```

## CLI Tool Integration

The migrations system includes utilities for building command-line migration tools. This allows you to create standalone executables for managing database schema evolution.

### Basic CLI Tool Setup

```cpp
#include <relx/migrations.hpp>
#include <relx/schema.hpp>

using namespace relx;

// Define your table versions
struct UsersV1 {
    static constexpr auto table_name = "users";
    
    column<UsersV1, "id", int, primary_key> id;
    column<UsersV1, "email", std::string> email;
    column<UsersV1, "name", std::string> name;
    
    unique_constraint<&UsersV1::email> unique_email;
};

struct UsersV2 {
    static constexpr auto table_name = "users";
    
    column<UsersV2, "id", int, primary_key> id;
    column<UsersV2, "email", std::string> email;
    column<UsersV2, "full_name", std::string> full_name;  // renamed from name
    column<UsersV2, "age", std::optional<int>> age;       // new nullable column
    column<UsersV2, "created_at", std::string, string_default<"CURRENT_TIMESTAMP">> created_at;  // new with default
    column<UsersV2, "is_active", bool, default_value<true>> is_active;                 // new boolean with default
 
    unique_constraint<&UsersV2::email> unique_email;
};

struct UsersV3 {
    static constexpr auto table_name = "users";
    
    column<UsersV3, "id", int, primary_key> id;
    column<UsersV3, "email", std::string> email;
    column<UsersV3, "full_name", std::string> full_name;
    column<UsersV3, "birth_year", int> birth_year;        // changed from age to birth_year
    column<UsersV3, "created_at", std::string, 
           string_default<"CURRENT_TIMESTAMP", true>> created_at;
    
    unique_constraint<&UsersV3::email> unique_email;
    table_check_constraint<"birth_year > 1900 AND birth_year <= EXTRACT(YEAR FROM CURRENT_DATE)"> valid_birth_year;
};

// Migration generator functions
migrations::MigrationResult<migrations::Migration> generate_migration_between_versions(const std::string& from, const std::string& to) {
    migrations::MigrationOptions options;
    
    if (from == "v1" && to == "v2") {
        // Handle name -> full_name rename
        options.column_mappings = {{"name", "full_name"}};
        
        UsersV1 old_table;
        UsersV2 new_table;
        return migrations::generate_migration(old_table, new_table, options);
    }
    else if (from == "v2" && to == "v3") {
        // Handle age -> birth_year transformation
        options.column_mappings = {{"age", "birth_year"}};
        options.column_transformations = {
            {"age", {
                "EXTRACT(YEAR FROM CURRENT_DATE) - age",  // forward: age -> birth_year
                "EXTRACT(YEAR FROM CURRENT_DATE) - birth_year"  // backward: birth_year -> age
            }}
        };
        
        UsersV2 old_table;
        UsersV3 new_table;
        return migrations::generate_migration(old_table, new_table, options);
    }
    else {
        return std::unexpected(migrations::MigrationError::make(
            migrations::MigrationErrorType::UNSUPPORTED_OPERATION,
            "Unsupported migration path: " + from + " -> " + to
        ));
    }
}

migrations::MigrationResult<migrations::Migration> generate_create_migration(const std::string& version) {
    if (version == "v1") {
        UsersV1 table;
        return migrations::generate_create_table_migration(table);
    }
    else if (version == "v2") {
        UsersV2 table;
        return migrations::generate_create_table_migration(table);
    }
    else if (version == "v3") {
        UsersV3 table;
        return migrations::generate_create_table_migration(table);
    }
    else {
        return std::unexpected(migrations::MigrationError::make(
            migrations::MigrationErrorType::UNSUPPORTED_OPERATION,
            "Unsupported version: " + version
        ));
    }
}

migrations::MigrationResult<migrations::Migration> generate_drop_migration(const std::string& version) {
    if (version == "v1") {
        UsersV1 table;
        return migrations::generate_drop_table_migration(table);
    }
    else if (version == "v2") {
        UsersV2 table;
        return migrations::generate_drop_table_migration(table);
    }
    else if (version == "v3") {
        UsersV3 table;
        return migrations::generate_drop_table_migration(table);
    }
    else {
        return std::unexpected(migrations::MigrationError::make(
            migrations::MigrationErrorType::UNSUPPORTED_OPERATION,
            "Unsupported version: " + version
        ));
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> supported_versions = {"v1", "v2", "v3"};
    
    // Full functionality CLI tool with CREATE, DROP, and GENERATE capabilities
    return migrations::cli::run_migration_tool(
        argc, argv,
        supported_versions,
        generate_migration_between_versions,
        generate_create_migration,        // Optional: provides --create functionality
        generate_drop_migration           // Optional: provides --drop functionality
    );
}
```

### CLI Tool Usage Patterns

The CLI utility supports several configuration patterns:

#### Full Functionality (Recommended)
```cpp
// Provides --generate, --create, and --drop commands
return migrations::cli::run_migration_tool(
    argc, argv,
    supported_versions,
    generate_migration_between_versions,
    generate_create_migration,
    generate_drop_migration
);
```

#### Generate-Only Tool
```cpp
// Only --generate command available
// --create and --drop commands will throw errors
return migrations::cli::run_migration_tool(
    argc, argv,
    supported_versions,
    generate_migration_between_versions
);
```

#### Create-Only Tool
```cpp
// Only --generate and --create commands available
// --drop command will throw error
return migrations::cli::run_migration_tool(
    argc, argv,
    supported_versions,
    generate_migration_between_versions,
    generate_create_migration
);
```

### Command Line Usage

Once built, your migration tool supports these commands:

```bash
# Generate migration between versions
./migration_tool --generate --from v1 --to v2

# Create a new table for a specific version
./migration_tool --create --version v2

# Generate drop migration for a specific version
./migration_tool --drop --version v2

# Show help
./migration_tool --help
```

The `relx::migrations` system provides a powerful, type-safe way to manage database schema evolution in C++. By leveraging compile-time table definitions and automatic migration generation, it reduces the risk of schema migration errors while providing full control over the migration process. 