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

## Quick Start - CLI Tool

The fastest way to get started is by building a command-line migration tool. This provides a complete interface for managing database schema evolution.

### Basic CLI Tool Setup

```cpp
#include <relx/migrations.hpp>
#include <relx/schema.hpp>

using namespace relx;

// Define your table versions
// clang-format off
struct [[=relx::table("users")]] UsersV1 {
    [[=relx::ann::pk]] int id;
    [[=relx::ann::unique]] std::string email;
    std::string name;
};

struct [[=relx::table("users")]] UsersV2 {
    [[=relx::ann::pk]] int id;
    [[=relx::ann::unique]] std::string email;
    std::string full_name;                                        // renamed from name
    std::optional<int> age;                                       // new nullable column
    [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
    [[=relx::default_value<true>{}]] bool is_active;
};

struct [[=relx::table("users"),
        =relx::ann::check("birth_year > 1900 AND birth_year <= EXTRACT(YEAR FROM CURRENT_DATE)")
             .named("valid_birth_year")]] UsersV3 {
    [[=relx::ann::pk]] int id;
    [[=relx::ann::unique]] std::string email;
    std::string full_name;
    int birth_year;                                               // replaces age
    [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
};
// clang-format on

// Migration generator functions
migrations::MigrationResult<migrations::Migration> generate_migration_between_versions(const std::string& from, const std::string& to) {
    migrations::MigrationOptions options;
    
    if (from == "v1" && to == "v2") {
        // Handle name -> full_name rename
        options.column_mappings = {{"name", "full_name"}};
        
        return migrations::generate_migration(relx::t<UsersV1>, relx::t<UsersV2>, options);
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
        
        return migrations::generate_migration(relx::t<UsersV2>, relx::t<UsersV3>, options);
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
        return migrations::generate_create_table_migration(relx::t<UsersV1>);
    }
    else {
        return std::unexpected(migrations::MigrationError::make(
            migrations::MigrationErrorType::UNSUPPORTED_OPERATION,
            "Unsupported version: " + version
        ));
    }
}

migrations::MigrationResult<migrations::Migration> generate_drop_migration(const std::string& version) {
    if (version == "v3") {
        return migrations::generate_drop_table_migration(relx::t<UsersV3>);
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

## Programmatic API Usage

If you prefer to use the migration system programmatically without the CLI, here's how:

```cpp
#include <relx/migrations.hpp>
#include <relx/schema.hpp>

// Define your table versions
// clang-format off
struct [[=relx::table("users")]] UsersV1 {
    [[=relx::ann::pk]] int id;
    std::string name;
    [[=relx::ann::unique]] std::string email;
};

struct [[=relx::table("users")]] UsersV2 {
    [[=relx::ann::pk]] int id;
    std::string name;
    [[=relx::ann::unique]] std::string email;
    std::optional<int> age;                                       // new nullable column
    [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
};
// clang-format on

int main() {
    // generate_migration and the SQL accessors all return std::expected
    auto migration = relx::migrations::generate_migration(relx::t<UsersV1>, relx::t<UsersV2>);
    if (!migration) {
        std::println("{}", migration.error().format());
        return 1;
    }

    auto forward_sqls = migration->forward_sql();
    for (const auto& sql : *forward_sqls) {
        std::println("Forward: {}", sql);
    }
    // Forward: ALTER TABLE users ADD COLUMN age INTEGER;
    // Forward: ALTER TABLE users ADD COLUMN created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP;

    auto rollback_sqls = migration->rollback_sql();
    for (const auto& sql : *rollback_sqls) {
        std::println("Rollback: {}", sql);
    }
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
MigrationResult<Migration> generate_migration(const OldTable& old_table, const NewTable& new_table,
                                              const MigrationOptions& options = {});
```

**Parameters:**
- `old_table`, `new_table`: table objects — `relx::t<UsersV1>`, `relx::t<UsersV2>`
- `options`: optional column mappings and transformations

**Returns:** `std::expected<Migration, MigrationError>`. Both structs must resolve to the same SQL
table name; a mismatch is a `static_assert`, not a runtime error.

#### `generate_create_table_migration(table)`

Generates a migration to create a new table.

```cpp
template <schema::TableConcept Table>
MigrationResult<Migration> generate_create_table_migration(const Table& table);
```

#### `generate_drop_table_migration(table)`

Generates a migration to drop an existing table.

```cpp
template <schema::TableConcept Table>
MigrationResult<Migration> generate_drop_table_migration(const Table& table);
```

### Migration Class

The `Migration` class contains a sequence of migration operations and provides methods to generate SQL.

```cpp
class Migration {
public:
    // Forward migration SQL statements
    MigrationResult<std::vector<std::string>> forward_sql() const;

    // Rollback migration SQL statements (operations in reverse order)
    MigrationResult<std::vector<std::string>> rollback_sql() const;

    const std::string& name() const;
    bool empty() const;
    size_t size() const;
};
```

An operation whose SQL cannot be produced (an unsupported type change, for instance) surfaces as the
error side of the `expected` rather than a partially-generated statement list.

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
| `ADD_CONSTRAINT` | Add table constraint | `ALTER TABLE users ADD CONSTRAINT users_unique_0 UNIQUE (email);` |
| `DROP_CONSTRAINT` | Remove table constraint | `ALTER TABLE users DROP CONSTRAINT constraint_name;` |
| `UPDATE_DATA` | Transform column data | `UPDATE users SET new_col = transform(old_col);` |

## Advanced Usage Patterns

### Column Renaming

Handle column renames without data loss using column mappings:

```cpp
// clang-format off
// Old table structure
struct [[=relx::table("employees")]] EmployeesV1 {
    std::string first_name;
    std::string email_addr;
};

// New table structure with renamed columns
struct [[=relx::table("employees")]] EmployeesV2 {
    std::string given_name;  // renamed
    std::string email;       // renamed
};
// clang-format on

// ❌ WITHOUT mappings - causes data loss (DROP + ADD)
auto migration_data_loss = relx::migrations::generate_migration(relx::t<EmployeesV1>, relx::t<EmployeesV2>);
// Generates: ADD email, ADD given_name, DROP email_addr, DROP first_name

// ✅ WITH mappings - preserves data (RENAME)
relx::migrations::MigrationOptions options;
options.column_mappings = {
    {"first_name", "given_name"},
    {"email_addr", "email"}
};

auto migration_safe = relx::migrations::generate_migration(relx::t<EmployeesV1>, relx::t<EmployeesV2>, options);
auto safe_sqls = migration_safe->forward_sql();
// Output:
// ALTER TABLE employees RENAME COLUMN email_addr TO email;
// ALTER TABLE employees RENAME COLUMN first_name TO given_name;
```

### Column Type Changes with Data Transformation

Handle type changes while preserving and transforming data:

```cpp
// clang-format off
struct [[=relx::table("products")]] ProductsV1 {
    int price_cents;             // int cents
};

struct [[=relx::table("products")]] ProductsV2 {
    std::string price_dollars;   // string dollars
};
// clang-format on

relx::migrations::MigrationOptions options;
options.column_mappings = {{"price_cents", "price_dollars"}};
options.column_transformations = {
    {"price_cents", {
        "CAST(price_cents / 100.0 AS TEXT) || ' USD'",  // forward: cents -> dollars string
        "CAST(REPLACE(price_dollars, ' USD', '') AS DECIMAL) * 100"  // backward: dollars string -> cents
    }}
};

auto migration = relx::migrations::generate_migration(relx::t<ProductsV1>, relx::t<ProductsV2>, options);
auto forward_sqls = migration->forward_sql();
// Output:
// ALTER TABLE products ADD COLUMN price_dollars TEXT NOT NULL;
// UPDATE products SET price_dollars = CAST(price_cents / 100.0 AS TEXT) || ' USD';
// ALTER TABLE products DROP COLUMN price_cents;

auto rollback_sqls = migration->rollback_sql();
// Output (in reverse order):
// ALTER TABLE products ADD COLUMN price_cents INTEGER NOT NULL;
// UPDATE products SET price_cents = CAST(REPLACE(price_dollars, ' USD', '') AS DECIMAL) * 100;
// ALTER TABLE products DROP COLUMN price_dollars;
```

### Constraint Management

The system automatically handles constraint changes:

```cpp
// clang-format off
struct [[=relx::table("users")]] TableWithoutConstraints {
    [[=relx::ann::pk]] int id;
    std::string email;
};

// Multi-column constraints are struct-level (composite_unique); a single-column
// UNIQUE can be either a column annotation or composite_unique("col") — both diff
// as the same table-level constraint
struct [[=relx::table("users"), =relx::ann::composite_unique("email")]] TableWithConstraints {
    [[=relx::ann::pk]] int id;
    std::string email;
};
// clang-format on

auto migration = relx::migrations::generate_migration(relx::t<TableWithoutConstraints>,
                                                      relx::t<TableWithConstraints>);
auto forward_sqls = migration->forward_sql();
// Output: ALTER TABLE users ADD CONSTRAINT users_unique_0 UNIQUE (email);

auto rollback_sqls = migration->rollback_sql();
// Output: ALTER TABLE users DROP CONSTRAINT users_unique_0;
```

#### Column-level constraint modifiers diff as constraints too

`[[=relx::ann::unique]]`, `[[=relx::ann::fk<...>]]` (with any `on_delete`/`on_update` actions), and
`[[=relx::schema::check<"...">{}]]` all render *into the column definition* in DDL, but the differ
surfaces each as a table-level constraint (`UNIQUE (col)`, `FOREIGN KEY (col) REFERENCES ...`,
`CHECK (...)`) — so adding or removing one migrates as `ADD CONSTRAINT` / `DROP CONSTRAINT` and the
column (and its data) survives. A single-column PRIMARY KEY is the exception: it stays part of the
column definition, so changing it is a column rebuild — use `composite_pk("col")` if a pk must
participate in diffs as a constraint:

```cpp
// clang-format off
struct [[=relx::table("users")]] V1 { [[=relx::ann::pk]] int id; std::string email; };
struct [[=relx::table("users")]] V2 { [[=relx::ann::pk]] int id; [[=relx::ann::unique]] std::string email; };
// clang-format on
```

```sql
-- forward
ALTER TABLE users ADD CONSTRAINT users_unique_0 UNIQUE (email);
-- rollback
ALTER TABLE users DROP CONSTRAINT users_unique_0;
```

#### Generated constraint names are positional

Table-level constraints have no names of their own, so the differ generates
`<table>_pk`, `<table>_unique_<n>`, `<table>_fk_<n>`, `<table>_check_<n>`, `<table>_idx_<n>`, where
`n` is the constraint's index in **annotation order**. Reordering a struct's annotations therefore
renames its constraints, and a diff between the two orderings churns every one of them:

```sql
-- swapping =relx::ann::composite_unique(...) and =relx::ann::check(...) on the struct
ALTER TABLE orders ADD CONSTRAINT orders_check_1 CHECK (a > 0);
ALTER TABLE orders ADD CONSTRAINT orders_unique_0 UNIQUE (a, b);
ALTER TABLE orders DROP CONSTRAINT orders_check_1;
ALTER TABLE orders DROP CONSTRAINT orders_unique_0;
```

Treat annotation order as part of the schema: keep it stable across versions, and append new
constraint annotations rather than inserting them.

Constraints you name yourself with `.named("...")` are exempt: the explicit name is used as the
constraint's identity, so `DROP CONSTRAINT` targets the name that was actually created and
reordering does not rename them. Naming constraints is the way to opt out of positional churn:

```sql
-- from =relx::ann::check("a > 0").named("a_positive")
ALTER TABLE t ADD CONSTRAINT a_positive CHECK (a > 0);   -- forward
ALTER TABLE t DROP CONSTRAINT a_positive;                -- rollback
```

### Table Creation and Deletion

Create migrations for entirely new or removed tables:

```cpp
// clang-format off
struct [[=relx::table("analytics")]] Analytics {
    [[=relx::ann::pk]] int id;
    std::string event;
    std::string timestamp;
};
// clang-format on

// Create table migration
auto create_migration = relx::migrations::generate_create_table_migration(relx::t<Analytics>);
auto create_sqls = create_migration->forward_sql();
// Forward: CREATE TABLE analytics (
//           id INTEGER NOT NULL PRIMARY KEY,
//           event TEXT NOT NULL,
//           timestamp TEXT NOT NULL
//           );

auto create_rollback_sqls = create_migration->rollback_sql();
// Rollback: DROP TABLE IF EXISTS analytics;

// Drop table migration
auto drop_migration = relx::migrations::generate_drop_table_migration(relx::t<Analytics>);
auto drop_sqls = drop_migration->forward_sql();
// Forward: DROP TABLE IF EXISTS analytics;

auto drop_rollback_sqls = drop_migration->rollback_sql();
// Rollback: CREATE TABLE analytics (...);
```

## Best Practices

### 1. Always Test Migrations

Build and run tests for every migration to ensure they work correctly:

```cpp
// In your test file
TEST(MigrationTest, TestUsersMigrationV1ToV2) {
    auto migration = relx::migrations::generate_migration(relx::t<UsersV1>, relx::t<UsersV2>);
    ASSERT_TRUE(migration) << migration.error().format();

    EXPECT_FALSE(migration->empty());
    EXPECT_EQ(migration->size(), 2);  // Should add 2 columns

    auto forward_sqls = migration->forward_sql();
    ASSERT_TRUE(forward_sqls);
    EXPECT_EQ((*forward_sqls)[0], "ALTER TABLE users ADD COLUMN age INTEGER;");
    EXPECT_EQ((*forward_sqls)[1],
              "ALTER TABLE users ADD COLUMN created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP;");

    auto rollback_sqls = migration->rollback_sql();
    ASSERT_TRUE(rollback_sqls);
    EXPECT_EQ((*rollback_sqls)[0], "ALTER TABLE users DROP COLUMN created_at;");
    EXPECT_EQ((*rollback_sqls)[1], "ALTER TABLE users DROP COLUMN age;");
}
```

### 2. Use Column Mappings for Renames

Always use column mappings when renaming columns to avoid data loss:

```cpp
// ✅ Good - preserves data
relx::migrations::MigrationOptions options;
options.column_mappings = {{"old_name", "new_name"}};
auto safe_migration = relx::migrations::generate_migration(relx::t<OldUsers>, relx::t<NewUsers>, options);

// ❌ Bad - loses data
auto unsafe_migration = relx::migrations::generate_migration(relx::t<OldUsers>, relx::t<NewUsers>);
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
// Good naming convention - the SQL table name comes from the annotation, so every
// version can carry the same table("users") while the struct names differ
struct [[=relx::table("users")]] UsersV1 { /* ... */ };
struct [[=relx::table("users")]] UsersV2 { /* ... */ };
struct [[=relx::table("users")]] UsersV3 { /* ... */ };
```

### 5. Review Generated SQL

Always review the generated SQL before applying to production:

```cpp
auto migration = relx::migrations::generate_migration(relx::t<OldUsers>, relx::t<NewUsers>);

std::println("=== Migration: {} ===", migration->name());
std::println("Operations: {}", migration->size());

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

## Common Patterns

### Incremental Schema Evolution

```cpp
// Define evolution path
UsersV1 users_v1;
UsersV2 users_v2;  
UsersV3 users_v3;

// Generate incremental migrations
auto migration_v1_to_v2 = relx::migrations::generate_migration(relx::t<UsersV1>, relx::t<UsersV2>);
auto migration_v2_to_v3 = relx::migrations::generate_migration(relx::t<UsersV2>, relx::t<UsersV3>);

// Apply in sequence
apply_migration(migration_v1_to_v2);
apply_migration(migration_v2_to_v3);
```

### Conditional Migrations

```cpp
auto migration = relx::migrations::generate_migration(relx::t<OldUsers>, relx::t<NewUsers>);

if (!migration->empty()) {
    std::println("Applying migration: {}", migration->name());
    apply_migration(*migration);
} else {
    std::println("No migration needed - tables are identical");
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

1. **Check table names match**: both structs must resolve to the same SQL table name — `generate_migration` static_asserts on this
2. **Verify column definitions**: make sure column types and annotations are what you think they are
3. **Check the structs are aggregates**: reflection walks non-static data members, so no constructors, no private members

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

The `relx::migrations` system provides a powerful, type-safe way to manage database schema evolution in C++. By leveraging compile-time table definitions and automatic migration generation, it reduces the risk of schema migration errors while providing full control over the migration process. 