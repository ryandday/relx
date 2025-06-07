# relx Documentation

Welcome to the relx documentation. This documentation provides comprehensive guides and examples for using the relx C++ query building library.

## Quick Navigation

| Guide | Description |
|-------|-------------|
| [Schema Definition](schema-definition.md) | Define tables, columns, and relationships |
| [Query Building](query-building.md) | Build SQL queries with the fluent API |
| [Result Parsing](result-parsing.md) | Process query results in a type-safe way |
| [Error Handling](error-handling.md) | Working with errors and expected results |
| [Advanced Examples](advanced-examples.md) | Complex query patterns and real-world examples |
| [Performance Guide](performance.md) | Optimization tips and best practices |
| [Development Guide](development.md) | Building, testing, and contributing |

## Library Overview

relx is a modern C++23 library for building and executing SQL queries with compile-time type safety. Key capabilities:

- **Schema Definition**: Define database schemas as C++ structures
- **Type-Safe Queries**: Build SQL queries with a fluent, chainable API
- **Compile-Time Validation**: Catch SQL errors at compile time
- **Database Execution**: Execute queries against PostgreSQL databases
- **Result Mapping**: Process results with automatic type conversion

## Getting Started Example

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>
#include <iostream>

// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    
    relx::unique_constraint<&Users::email> unique_email;
};

// Define a DTO for query results
struct UserDTO {
    int id;
    std::string name;
    std::string email;
};

int main() {
    // Database connection
    relx::PostgreSQLConnectionParams params{
        .host = "localhost",
        .port = 5432,
        .dbname = "example",
        .user = "postgres",
        .password = "postgres",
        .application_name = "relx_example"
    };
    
    relx::PostgreSQLConnection conn(params);
    
    auto connect_result = conn.connect();
    if (!connect_result) {
        std::println("Connection error: {}", connect_result.error().message);
        return 1;
    }
    
    Users users;
    
    // Create table
    auto create_result = conn.execute(relx::create_table(users).if_not_exists());
    relx::throw_if_failed(create_result);
    
    // Insert data
    auto insert_result = conn.execute(
        relx::insert_into(users)
            .columns(users.name, users.email)
            .values("John Doe", "john@example.com")
    );
    relx::throw_if_failed(insert_result);
    
    // Query data with automatic mapping to DTO
    auto query = relx::select(users.id, users.name, users.email)
        .from(users)
        .where(users.name.like("%John%"));
    
    auto result = conn.execute<UserDTO>(query);
    if (result) {
        for (const auto& user : *result) {
            std::println("User: {} - {} ({})", user.id, user.name, user.email);
        }
    }
    
    return 0;
}
```

## Architecture Overview

relx is designed with several key components:

- **Core Library** (header-only): Schema definitions, query building, SQL generation
- **PostgreSQL Client**: Synchronous database operations
- **PostgreSQL Async Client**: Asynchronous operations with Boost.Asio
- **Connection Pool**: Managed connection pooling for high-concurrency applications

## Next Steps

1. Start with [Schema Definition](schema-definition.md) to learn how to define your database structure
2. Read [Query Building](query-building.md) to understand the fluent API
3. Review [Result Parsing](result-parsing.md) for handling query results
4. Check [Error Handling](error-handling.md) for robust error management
5. Explore [Advanced Examples](advanced-examples.md) for complex scenarios
