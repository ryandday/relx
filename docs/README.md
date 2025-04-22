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
3. Execute queries against various database engines (SQLite, PostgreSQL)
4. Process results in a type-safe manner

## Example

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/sqlite.hpp>
#include <iostream>

// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"email", std::string> email;
    
    relx::schema::primary_key<&Users::id> pk;
};

int main() {
    // Create connection
    relx::SQLiteConnection conn(":memory:");
    conn.connect();
    
    // Create table
    Users users;
    conn.execute_raw("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, email TEXT NOT NULL)");
    
    // Insert data
    auto insert = relx::query::insert_into(users)
        .values(
            relx::query::set(users.id, 1),
            relx::query::set(users.name, "John Doe"),
            relx::query::set(users.email, "john@example.com")
        );
    conn.execute(insert);
    
    // Query data
    auto query = relx::query::select(users.id, users.name)
        .from(users)
        .where(users.id == 1);
    
    auto result = conn.execute(query);
    if (result) {
        for (const auto& row : *result) {
            auto id = row.get<int>("id");
            auto name = row.get<std::string>("name");
            if (id && name) {
                std::cout << "User: " << *id << " - " << *name << std::endl;
            }
        }
    }
    
    return 0;
}
```

## Additional Resources

- [GitHub Repository](https://github.com/yourusername/relx)
- [API Reference](api-reference.md)
- [Contributing Guidelines](contributing.md) 