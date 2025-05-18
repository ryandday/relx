# relx Documentation

Welcome to the relx (Relational X) documentation. This documentation provides comprehensive guides and examples for using the relx C++ query building library.

## Table of Contents

- [Getting Started](getting-started.md): Basic introduction and setup
- [Schema Definition](schema-definition.md): How to define tables and columns
- [Query Building](query-building.md): Building SQL queries with the fluent API
- [Query Execution](query-execution.md): Executing queries and processing results
- [Advanced Examples](advanced-examples.md): Complex query patterns and examples
- [Error Handling](error-handling.md): Working with errors and expected results
- [Transactions](transactions.md): Transaction management

## Library Overview

relx is a modern C++23 library for building and executing SQL queries with compile-time type safety. It allows you to:

1. Define database schemas as C++ structures
2. Build SQL queries with a fluent, type-safe API
3. Execute queries against PostgreSQL databases
4. Process results in a type-safe manner

## Example

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/postgresql.hpp>
#include <iostream>

// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<"id", int> id;
    relx::column<"name", std::string> name;
    relx::column<"email", std::string> email;
    
    relx::primary_key<&Users::id> pk;
};

int main() {
    // Create connection
    relx::PostgreSQLConnection conn("host=localhost port=5432 dbname=example user=postgres password=postgres");
    
    // Connect to the database
    auto connect_result = conn.connect();
    if (!connect_result) {
        std::println("Connection error: {}", connect_result.error().message);
        return 1;
    }
    
    // Create table
    Users users;
    conn.execute_raw(relx::create_table(users));
    
    // Insert data
    auto insert = relx::insert_into(users)
        .values(
            relx::set(users.name, "John Doe"),
            relx::set(users.email, "john@example.com")
        )
        .returning(users.id);  // PostgreSQL supports RETURNING clause
        
    auto insert_result = conn.execute(insert);
    if (!insert_result) {
        std::println("Insert error: {}", insert_result.error().message);
        return 1;
    }
    
    int user_id = (*insert_result)[0].get<int>("id").value_or(0);
    std::println("Inserted user with ID: {}", user_id);
    
    // Query data
    auto query = relx::select(users.id, users.name)
        .from(users)
        .where(users.id == user_id);
    
    auto result = conn.execute(query);
    if (result) {
        for (const auto& row : *result) {
            auto id = row.get<int>("id");
            auto name = row.get<std::string>("name");
            if (id && name) {
                std::println("User: {} - {}", *id, *name);
            }
        }
    }
    
    // Clean up
    conn.execute_raw(relx::drop_table(users));
    
    // Disconnect
    conn.disconnect();
    
    return 0;
}
```

## Additional Resources

- [GitHub Repository](https://github.com/yourusername/relx)
- [API Reference](api-reference.md)
- [Contributing Guidelines](contributing.md) 