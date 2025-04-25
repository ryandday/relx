# relx - Modern C++ SQL Query Building Library

relx is a modern C++23 library for constructing and executing SQL queries with compile-time type safety. It provides a fluent, intuitive interface for building SQL queries while preventing SQL injection and type errors.

## Key Features

- **Type Safety**: SQL queries are validated at compile time
- **Fluent Interface**: Build SQL using intuitive, chainable method calls
- **Schema Definition**: Strongly typed table and column definitions
- **Query Building**: Type-safe SELECT, INSERT, UPDATE, and DELETE operations
- **Compile-time Validation**: Catch SQL syntax errors during compilation when possible

## Requirements

- C++23 compiler support
  - GCC 11+
  - Clang 14+
  - MSVC 19.29+ (Visual Studio 2022+)
- CMake 3.15+
- Conan 2.0+ (for dependencies)

## Dependencies

- Boost (header-only)
- fmt
- Google Test (for testing)

## Getting Started

### Building the Project

#### Install conan

```bash
# Clone the repository
git clone https://github.com/ryandday/relx.git
cd relx

# Build using make
make build

# Run tests
make test
```

### Basic Usage

```cpp
#include <relx/schema.hpp>
#include <iostream>

// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"username", std::string> username;
    relx::schema::column<"email", std::string> email;
    
    // Primary key
    relx::schema::primary_key<&Users::id> pk;
    
    // Unique constraints
    relx::schema::unique_constraint<&Users::email> unique_email;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"user_id", int> user_id;
    relx::schema::column<"title", std::string> title;
    relx::schema::column<"content", std::string> content;
    
    // Primary key
    relx::schema::primary_key<&Posts::id> pk;
    
    // Foreign key
    relx::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

int main() {
    Users users;
    Posts posts;
    
    // Building a simple SELECT query
    auto query = relx::query::select(users.id, users.username)
        .from(users)
        .where(users.id > 10);
    
    // Generate the SQL
    std::string sql = query.to_sql();
    // Output: "SELECT id, username FROM users WHERE (id > ?)"
    
    // Get the bind parameters
    auto params = query.bind_params();
    // params[0] = "10"
    
    // Connect to a database
    relx::SQLiteConnection conn("my_database.db");
    conn.connect();
    
    // Execute the query
    auto result = conn.execute(query);
    if (result) {
        // Process results
        for (const auto& row : *result) {
            auto id = row.get<int>("id");
            auto username = row.get<std::string>("username");
            if (id && username) {
                std::cout << "User: " << *id << " - " << *username << std::endl;
            }
        }
    }
    
    return 0;
}
```

### Query Building Examples

#### SELECT Queries

```cpp
Users users;
Posts posts;

// Basic SELECT
auto query1 = relx::query::select(users.id, users.username, users.email)
    .from(users);

// SELECT with WHERE condition
auto query2 = relx::query::select(users.id, users.username)
    .from(users)
    .where(users.id > 10);

// SELECT with multiple conditions
auto query3 = relx::query::select(users.id, users.username)
    .from(users)
    .where(users.id > 10 && users.username.like("%john%"));

// SELECT with ORDER BY and LIMIT
auto query4 = relx::query::select(users.id, users.username)
    .from(users)
    .order_by(relx::query::desc(users.id))
    .limit(10);

// SELECT with JOIN
auto query5 = relx::query::select(users.username, posts.title)
    .from(users)
    .join(posts, relx::query::on(users.id == posts.user_id));

// SELECT with GROUP BY and aggregates
auto query6 = relx::query::select_expr(
    users.id,
    users.username,
    relx::query::as(relx::query::count(posts.id), "post_count")
).from(users)
 .join(posts, relx::query::on(users.id == posts.user_id))
 .group_by(users.id, users.username)
 .having(relx::query::count(posts.id) > 5);
```

#### INSERT Queries

```cpp
Users users;

// Simple INSERT
auto insert1 = relx::query::insert_into(users)
    .values(
        relx::query::set(users.username, "john_doe"),
        relx::query::set(users.email, "john@example.com")
    );

// Multi-row INSERT
auto insert2 = relx::query::insert_into(users)
    .columns(users.username, users.email)
    .values("john_doe", "john@example.com")
    .values("jane_smith", "jane@example.com");
```

#### UPDATE Queries

```cpp
Users users;

// Basic UPDATE
auto update1 = relx::query::update(users)
    .set(
        relx::query::set(users.username, "new_username"),
        relx::query::set(users.email, "new_email@example.com")
    )
    .where(users.id == 1);
```

#### DELETE Queries

```cpp
Users users;

// Basic DELETE
auto delete1 = relx::query::delete_from(users)
    .where(users.id == 1);

// DELETE all
auto delete2 = relx::query::delete_from(users);
```

For more advanced examples, see our [documentation](docs/README.md).

## Schema Features

relx provides a rich set of schema definition features:

- **Tables**: Define database tables with strongly typed columns
- **Columns**: Specify column names, types, and constraints
- **Primary Keys**: Single-column and composite primary keys
- **Foreign Keys**: Define relationships between tables
- **Indexes**: Create regular and unique indexes
- **Constraints**: Check constraints, unique constraints, default values
- **Nullable Columns**: Support for NULL values via std::optional

## Advanced Features

- **Compile-time SQL Generation**: SQL strings generated at compile time where possible
- **Type-safe Parameter Binding**: Automatic parameter binding with correct types
- **Transaction Support**: RAII-based transaction management
- **Connection Pooling**: Efficient database connection management
- **Result Set Processing**: Strongly typed result access

## Supported Databases

### SQLite

relx provides native support for SQLite databases through the `relx::SQLiteConnection` class.

Example:

```cpp
#include <relx/sqlite.hpp>

// Create a connection to an SQLite database
relx::SQLiteConnection conn("my_database.db");
conn.connect();

// Execute a query
auto result = conn.execute_raw("SELECT * FROM users");
if (result) {
    // Process results
    for (const auto& row : *result) {
        auto name = row.get<std::string>("name");
        if (name) {
            std::cout << "Name: " << *name << std::endl;
        }
    }
}

// Disconnect when done
conn.disconnect();
```

### PostgreSQL

relx also provides support for PostgreSQL databases through the `relx::PostgreSQLConnection` class.

Example:

```cpp
#include <relx/postgresql.hpp>

// Create a connection to a PostgreSQL database
relx::PostgreSQLConnection conn("host=localhost port=5432 dbname=my_database user=postgres password=postgres");
conn.connect();

// Execute a query
auto result = conn.execute_raw("SELECT * FROM users");
if (result) {
    // Process results
    for (const auto& row : *result) {
        auto name = row.get<std::string>("name");
        if (name) {
            std::cout << "Name: " << *name << std::endl;
        }
    }
}

// Disconnect when done
conn.disconnect();
```

## Docker Development Environment

relx provides Docker Compose configuration for development and testing.

### PostgreSQL

To start a PostgreSQL container for testing:

```bash
make postgres-up
```

To stop the PostgreSQL container:

```bash
make postgres-down
```

To view PostgreSQL logs:

```bash
make postgres-logs
```

To remove the PostgreSQL container and volumes:

```bash
make postgres-clean
```

The tests automatically start the necessary database containers.

## Documentation

For more detailed documentation, check the following:

- [Schema Example](examples/schema_example.cpp): Complete example of schema definition
- [DB Connection API](DB_CONNECTION_API.md): Database connection API documentation
- [Coding Standards](CODING_STANDARDS.md): C++ coding standards for this project

## Project Structure

- `include/relx/`: Header files
- `src/`: Implementation files
- `examples/`: Usage examples
- `tests/`: Unit tests
- `build/`: Build output (created during build)

## Contributing

Contributions are welcome! Please follow our [Coding Standards](CODING_STANDARDS.md) when making changes.

## License

[MIT License](LICENSE)
