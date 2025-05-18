# relx Documentation

Welcome to the relx (Relational X) documentation. This documentation provides comprehensive guides and examples for using the relx C++ query building library.

## Table of Contents

- [Schema Definition](schema-definition.md): How to define tables and columns
- [Query Building](query-building.md): Building SQL queries with the fluent API
- [Result Parsing](result-parsing.md): Processing query results in a type-safe way
- [Advanced Examples](advanced-examples.md): Complex query patterns and examples
- [Error Handling](error-handling.md): Working with errors and expected results

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
    
    relx::column<Users, "id", int> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    
    relx::primary_key<&Users::id> pk;
};

// Define a DTO (Data Transfer Object) for User data
struct UserDTO {
    int id;
    std::string name;
    std::string email;
};

int main() {
    // Create connection using a struct
    relx::ConnectionParams params{
        .host = "localhost",
        .port = 5432,
        .dbname = "example",
        .user = "postgres",
        .password = "postgres"
    };
    
    relx::PostgreSQLConnection conn(params);
    
    // Connect to the database
    auto connect_result = conn.connect();
    if (!connect_result) {
        std::println("Connection error: {}", connect_result.error().message);
        return 1;
    }
    
    // Create table
    Users users;
    auto create_table_query = relx::create_table(users);
    conn.execute(create_table_query);
    
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
    auto query = relx::select(users.id, users.name, users.email)
        .from(users)
        .where(users.id == user_id);
    
    auto result = conn.execute<UserDTO>(query);
    if (result) {
        for (const auto& user : *result) {
            std::println("User: {} - {} ({})", user.id, user.name, user.email);
        }
    }
    
    // Clean up
    auto drop_table_query = relx::drop_table(users);
    conn.execute(drop_table_query);
    
    // Disconnect
    conn.disconnect();
    
    return 0;
}
```
