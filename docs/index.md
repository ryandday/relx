# relx Documentation

Welcome to the **relx** documentation. relx is a modern C++23 library that provides type-safe SQL query building and execution with compile-time validation.

<div class="alert info">
<strong>✨ New to relx?</strong> Start with the <a href="schema-definition.html">Schema Definition</a> guide to learn the basics, then move on to <a href="query-building.html">Query Building</a>.
</div>

<div class="nav-list">
<h2>📚 Documentation Guide</h2>

### Core Concepts
- [**Schema Definition**](schema-definition.html) - Define tables, columns, and relationships
- [**Query Building**](query-building.html) - Build SQL queries with the fluent API  
- [**Result Parsing**](result-parsing.html) - Process query results in a type-safe way
- [**Error Handling**](error-handling.html) - Working with errors and expected results

### Advanced Topics
- [**Advanced Examples**](advanced-examples.html) - Complex query patterns and real-world examples
- [**Performance Guide**](performance.html) - Optimization tips and best practices

### Development
- [**Development Guide**](development.html) - Building, testing, and contributing to relx
</div>

---

## 🎯 Key Features

<table>
<thead>
<tr>
<th>Feature</th>
<th>Description</th>
<th>Benefit</th>
</tr>
</thead>
<tbody>
<tr>
<td><strong>Schema Definition</strong></td>
<td>Define database schemas as C++ structures</td>
<td>Single source of truth, compile-time validation</td>
</tr>
<tr>
<td><strong>Type-Safe Queries</strong></td>
<td>Build SQL queries with a fluent, chainable API</td>
<td>Catch errors at compile time, not runtime</td>
</tr>
<tr>
<td><strong>Automatic Mapping</strong></td>
<td>Map query results directly to C++ types</td>
<td>Reduce boilerplate, eliminate mapping errors</td>
</tr>
<tr>
<td><strong>Modern C++23</strong></td>
<td>Uses concepts, ranges, and latest language features</td>
<td>Clean, expressive, and performant code</td>
</tr>
</tbody>
</table>

---

## 🚀 Quick Start Example

Here's a complete example showing how to define a schema, create tables, and query data:

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/postgresql.hpp>
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

<hr class="section-divider">

## 🏗️ Architecture Overview

relx is designed with modularity and performance in mind:

<div class="alert success">
<strong>Header-Only Core:</strong> The core query building and schema definition components are header-only for easy integration and optimal compile-time optimization.
</div>

### Components

- **🎯 Core Library** (header-only): Schema definitions, query building, SQL generation
- **🐘 PostgreSQL Client**: Synchronous database operations with libpq
- **⚡ PostgreSQL Async Client**: Asynchronous operations with Boost.Asio  
- **🔄 Connection Pool**: Managed connection pooling for high-concurrency applications

### Design Principles

- **Compile-time Safety**: Catch SQL errors before they reach production
- **Zero-cost Abstractions**: High-level API with no runtime overhead
- **Modern C++**: Leverages C++23 features for clean, expressive code
- **Database Agnostic Core**: Easy to extend for other database backends

<hr class="section-divider">

## 📖 Learning Path

<div class="alert info">
<strong>Recommended Learning Order:</strong>
</div>

1. **[Schema Definition](schema-definition.html)** - Learn how to define your database structure as C++ types
2. **[Query Building](query-building.html)** - Master the fluent API for building type-safe queries  
3. **[Result Parsing](result-parsing.html)** - Understand how to work with query results
4. **[Error Handling](error-handling.html)** - Build robust applications with proper error management
5. **[Advanced Examples](advanced-examples.html)** - Explore complex real-world scenarios

<div class="alert warning">
<strong>Prerequisites:</strong> This library requires C++23 support. Make sure you have GCC 11+, Clang 14+, or MSVC 19.29+.
</div>

---

Ready to get started? Head over to the [**Schema Definition**](schema-definition.html) guide! 🎉 