# relx

[![CI](https://github.com/ryandday/relx/workflows/CI/badge.svg)](https://github.com/ryandday/relx/actions)
[![codecov](https://codecov.io/gh/ryandday/relx/branch/main/graph/badge.svg)](https://codecov.io/gh/ryandday/relx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/26)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-supported-336791.svg)](https://www.postgresql.org/)

## Modern C++ SQL Query Building Library

> 🚀 **Transform your SQL experience** - Build type-safe, compile-time validated SQL queries with modern C++26

Working with SQL often means writing error prone raw strings. Refactoring is a pain, and in more complex applications you have to write extensive tests to make sure your stringly typed queries work in every situation.

relx is a modern C++26 library designed to solve these problems by constructing and executing SQL queries with compile-time type safety. It provides a fluent, intuitive interface for building SQL queries while preventing SQL injection and type errors. 

The core library is header-only. relx also features two PostgreSQL clients for database access: an async client (with `boost::asio`) and a synchronous client. The PostgreSQL clients are not header-only and require building with [libpq](https://www.postgresql.org/docs/current/libpq.html).

This library is a work in progress. Contributions, issues, and usability feedback are welcome! Feel free to open an issue about any concern.

## Motivation 

This library was born from my desire for a C++ SQL library that offers the expressiveness of high-level languages without sacrificing C++'s performance and control.
I didn't want crazy template parameters, macros, or member pointers, just a simple builder API. 
I also wanted a couple features out-of-the-box: async capabilities, because async is the only practical approach for web applications handling massive amounts of concurrent users, lazy loading of rows in case I have a big query, and some migration utilities for convenience.

## Table of Contents

- [Key Features](#key-features)
- [Documentation](#documentation)
- [Quick Start](#quick-start)
- [Schema Features](#schema-features)
- [Advanced Features](#advanced-features)
- [Supported Databases](#supported-databases)
- [Docker Development Environment](#docker-development-environment)
- [License](#license)

## Key Features

| Feature | Description | Benefit |
|---------|-------------|---------|
| **Type Safety** | SQL queries are validated at compile time | Catch errors before runtime, no more SQL typos |
| **Fluent Interface** | Build SQL using intuitive, chainable method calls | Write readable, maintainable query code |
| **Schema Definition** | Strongly typed table and column definitions | Database schema as code with full IntelliSense |
| **Query Building** | Type-safe SELECT, INSERT, UPDATE, and DELETE operations | Zero SQL injection risk, compile-time validation |
| **Boilerplate/Macro Free** | Uses C++26 reflection (P2996) for schema definitions and result mapping | Clean, readable code without preprocessor magic |
| **Header-Only Core** | Core library requires no compilation | Easy integration, fast build times |

> **Built on C++26 reflection**: relx uses standard reflection (P2996) instead of Boost::pfr. See [docs/cpp26-reflection-plan.md](docs/cpp26-reflection-plan.md) for the adoption roadmap.

## Documentation

- **[User Guides](docs/)** - Comprehensive documentation and examples
- **[API Documentation](https://ryandday.github.io/relx/)** - Auto-generated from source code

## Quick Start

### Requirements

- GCC 16.1+ with `-std=c++26 -freflection` (the only released compiler with C++26 reflection; Clang support expected ~Clang 24)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install libboost-all-dev
sudo apt-get install libpq-dev # Required if you plan to use PostgreSQL client libraries
```

**macOS (Homebrew):**
```bash
brew install boost
brew install postgresql # Required if you plan to use PostgreSQL client libraries
```

### Using relx in Your Project

relx can be easily integrated into your C++ project using CMake FetchContent:

```cmake
# Fetch relx
include(FetchContent)
FetchContent_Declare(
    relx
    GIT_REPOSITORY https://github.com/ryandday/relx.git
    GIT_TAG main  # or specify a specific tag/commit
)

set(RELX_ENABLE_POSTGRES_CLIENT ON)  # Enable PostgreSQL client library
FetchContent_MakeAvailable(relx)

# Your executable
add_executable(my_app main.cpp)
target_link_libraries(my_app relx::relx)
```

> **Note**: For alternative installation methods, building from source, running tests, and development instructions, see [docs/development.md](docs/development.md).

### Basic Usage

```cpp
#include <relx/schema.hpp>
#include <iostream>

// Define schemas: plain structs + C++26 annotations.
// Column names come from the member identifiers; the same struct is the result DTO.
// Next to each struct, define its table object once - reflection synthesizes one
// column member per field, so queries read naturally (users.id, users.username).
struct [[=relx::table("users")]] Users {
    [[=relx::ann::pk]] int id;
    std::string username;
    [[=relx::ann::unique]] std::string email;
};
inline constexpr auto users = relx::t<Users>;

struct [[=relx::table("posts")]] Posts {
    [[=relx::ann::pk]] int id;
    [[=relx::ann::fk<^^Users::id>]] int user_id;
    std::string title;
    std::string content;
};
inline constexpr auto posts = relx::t<Posts>;

int main() {
    try {
        // Connect to a database
        relx::PostgreSQLConnection conn({
            .host = "localhost",
            .port = 5432,
            .dbname = "mydb",
            .user = "postgres",
            .password = "postgres",
            .application_name = "myapp"
        });
        
        auto connect_result = conn.connect();
        relx::throw_if_failed(connect_result);

        auto create_table_result = conn.execute(relx::create_table(users).if_not_exists());
        relx::value_or_throw(create_table_result);

        auto insert_statement = relx::insert_into(users)
            .columns(users.username, users.email)
            .values("Jane Smith", "jane@example.com")
            .values("Bob Johnson", "bob@example.com");
        
        auto insert_result = conn.execute(insert_statement);
        relx::value_or_throw(insert_result);

        // Building a simple SELECT query
        auto query = relx::select(users.id, users.username)
            .from(users)
            .where(users.id > 10);
        
        // See the generated SQL
        std::string sql = query.to_sql();
        // Output: "SELECT users.id, users.username FROM users WHERE (users.id > ?)"
        
        // Get the bind parameters
        auto params = query.bind_params();
        // params[0] = "10"
        
        // Define a DTO to receive the results - fields are matched to result
        // columns BY NAME, so order doesn't matter
        struct UserDTO {
            int id;
            std::string username;
        };
        
        // Execute the query
        auto query_result = conn.execute_many<UserDTO>(query);
        const auto& rows = relx::value_or_throw(query_result);

        // Process results - automatically mapped to UserDTO objects
        for (const auto& user : rows) {
            std::println("User: {} - {}", user.id, user.username);
        }
        
        auto drop_result = conn.execute(relx::drop_table(users));
        relx::value_or_throw(drop_result);
        return 0;
    } catch (const relx::RelxException& e) {
        std::println("{}", e.what());
        return 1;
    }
}
```

### Creating and Dropping Tables

```cpp
// Runtime builders, executable through the connection
auto create_users = relx::create_table(users);
auto create_posts = relx::create_table(posts).if_not_exists();

auto drop_users = relx::drop_table(users).if_exists().cascade();
auto drop_posts = relx::drop_table(posts).if_exists();

// Or built entirely at compile time - to_sql() is consteval and returns a view of
// static storage, so the statement costs nothing at runtime
constexpr auto ddl = relx::create_table_sql<Users>().if_not_exists().to_sql();
```

### Query Building Examples

#### SELECT Queries

```cpp
// Basic SELECT
auto query1 = relx::select(users.id, users.username, users.email)
    .from(users);

// SELECT with WHERE condition
auto query2 = relx::select(users.id, users.username)
    .from(users)
    .where(users.id > 10);

// SELECT with multiple conditions
auto query3 = relx::select(users.id, users.username)
    .from(users)
    .where(users.id > 10 && users.username.like("%john%"));

// SELECT with ORDER BY and LIMIT
auto query4 = relx::select(users.id, users.username)
    .from(users)
    .order_by(relx::desc(users.id))
    .limit(10);

// SELECT with JOIN
auto query5 = relx::select(users.username, posts.title)
    .from(users)
    .join(posts, relx::on(users.id == posts.user_id));

// SELECT with GROUP BY and aggregates
auto query6 = relx::select_expr(
    users.id,
    users.username,
    relx::as(relx::count(posts.id), "post_count")
).from(users)
 .join(posts, relx::on(users.id == posts.user_id))
 .group_by(users.id, users.username)
 .having(relx::count(posts.id) > 5);
```

#### INSERT Queries

```cpp
// Simple INSERT
auto insert1 = relx::insert_into(users)
    .columns(users.username, users.email)
    .values("john_doe", "john@example.com");

// Multi-row INSERT
auto insert2 = relx::insert_into(users)
    .columns(users.username, users.email)
    .values("john_doe", "john@example.com")
    .values("jane_smith", "jane@example.com");
```

#### UPDATE Queries

```cpp
// Basic UPDATE
auto update1 = relx::update(users)
    .set(users.username, "new_username")
    .set(users.email, "new_email@example.com")
    .where(users.id == 1);
```

#### DELETE Queries

```cpp
// Basic DELETE
auto delete1 = relx::delete_from(users)
    .where(users.id == 1);

// DELETE all
auto delete2 = relx::delete_from(users);
```

For the full guides, see the [documentation](docs/README.md).

## Schema Features

relx provides a rich set of schema definition features:

| Feature | Example |
|---------|---------|
| **Tables** | `struct [[=relx::table("users")]] Users { ... };` |
| **Columns** | member identifiers and types: `std::string email;` |
| **Primary keys** | `[[=relx::ann::pk]] int id;` — composite: `=relx::ann::composite_pk("a", "b")` on the struct |
| **Foreign keys** | `[[=relx::ann::fk<^^Users::id>]] int user_id;` |
| **Unique** | `[[=relx::ann::unique]]` — composite: `=relx::ann::composite_unique("a", "b")` |
| **Indexes** | `=relx::ann::index_on("email").unique()` on the struct; emitted by `relx::create_indexes_sql<T>()` |
| **Check constraints** | column: `[[=relx::schema::check<"price > 0">{}]]`; table: `=relx::ann::check("a < b").named("ck")` |
| **Defaults** | `[[=relx::default_value<18>{}]]`, `[[=relx::string_default<"guest">{}]]`, `[[=relx::default_sql<"now()">{}]]` |
| **Nullable columns** | `std::optional<int> age;` |

See [Schema Definition](docs/schema-definition.md) for the full annotation vocabulary.

## Advanced Features

- **Transaction Support**: RAII-based transaction management
- **Connection Pooling**: Efficient database connection management  
- **Result Set Processing**: Strongly typed result access
- **Async Operations**: Non-blocking I/O with Boost.Asio
- **Schema Migrations**: Automatic DDL generation for database schema evolution
- **Type Safety**: Compile-time SQL validation

## Supported Databases

### PostgreSQL

relx provides robust support for PostgreSQL through several connection types to meet different requirements:

#### Standard Connection

The `PostgreSQLConnection` class provides a synchronous interface for PostgreSQL:

```cpp
#include <relx/connection.hpp>

// Create a connection to a PostgreSQL database
relx::PostgreSQLConnectionParams conn_params;
conn_params.host = "localhost";
conn_params.port = 5432;
conn_params.dbname = "my_database";
conn_params.user = "postgres";
conn_params.password = "postgres";
conn_params.application_name = "relx_example";

relx::PostgreSQLConnection conn(conn_params);
auto conn_result = conn.connect();

if (!conn_result) {
    std::println("Connection error: {}", conn_result.error().message);
    return 1;
}

// Define a DTO for the columns this query selects
struct UserDTO {
    std::string username;
};

// Build a type-safe query using the fluent API
auto query = relx::select(users.username)
    .from(users)
    .where(users.id > 5);

// Execute the query with automatic mapping to UserDTO
auto result = conn.execute_many<UserDTO>(query);
if (result) {
    // Process results - automatically mapped to UserDTO objects
    for (const auto& user : *result) {
        std::println("Name: {}", user.username);
    }
}

// Automatic disconnect of connection when it goes out of scope
```

#### Connection Pool

For applications requiring higher concurrency and performance, use the `PostgreSQLConnectionPool`:

```cpp
#include <relx/connection.hpp>

// Configure the connection pool
relx::PostgreSQLConnectionPoolConfig config{
    .connection_params = {
        .host = "localhost",
        .port = 5432,
        .dbname = "my_database",
        .user = "postgres",
        .password = "postgres",
        .application_name = "relx_pool_example"
    },
    .initial_size = 5,      // Start with 5 connections
    .max_size = 20,         // Allow up to 20 connections
    .connection_timeout = std::chrono::milliseconds(3000),
    .validate_connections = true,
    .max_idle_time = std::chrono::milliseconds(60000)
};

// Create the pool
auto pool = relx::PostgreSQLConnectionPool::create(config);
auto init_result = pool->initialize();

if (!init_result) {
    std::println("Pool initialization failed: {}", init_result.error().message);
    return 1;
}

// A DTO for the count result; one definition serves both patterns below
struct CountDTO {
    int count;
};

// Method 1: Get a connection from the pool
auto conn_result = pool->get_connection();
if (conn_result) {
    auto& conn = *conn_result;

    // Create a type-safe query
    auto query = relx::select_expr(relx::as(relx::count_all(), "count")).from(users);

    // Execute with automatic mapping to CountDTO
    auto query_result = conn->execute<CountDTO>(query);  // single row
    
    if (query_result) {
        std::println("User count: {}", query_result->count);
    }
    
    // Connection automatically returned to pool when conn goes out of scope
}

// Method 2: Use the with_connection pattern
auto user_count = pool->with_connection([](auto& conn) -> relx::ConnectionResult<int> {
    auto query = relx::select_expr(relx::as(relx::count_all(), "count")).from(users);
    // conn is a generic lambda parameter, so the member template needs `template`
    auto result = conn->template execute<CountDTO>(query);

    if (!result) {
        return std::unexpected(result.error());
    }
    
    return result->count;
});

// with_connection wraps the callable's own result, so there are two layers to unwrap
if (user_count && *user_count) {
    std::println("User count: {}", **user_count);
}
```

#### Asynchronous Connection

For non-blocking I/O, use the `PostgreSQLAsyncConnection` with Boost.Asio:

```cpp
#include <relx/connection.hpp>
#include <boost/asio.hpp>

// Define a coroutine to connect and run queries
boost::asio::awaitable<void> run_async_queries(boost::asio::io_context& io_context) {

    // Create async connection
    relx::PostgreSQLConnectionParams conn_params;
    conn_params.host = "localhost";
    conn_params.port = 5432;
    conn_params.dbname = "my_database";
    conn_params.user = "postgres";
    conn_params.password = "postgres";
    conn_params.application_name = "relx_async_example";
    
    relx::PostgreSQLAsyncConnection conn(
        io_context, 
        conn_params
    );

    // Connect
    auto connect_result = co_await conn.connect();
    if (!connect_result) {
        std::println("Connect error: {}", connect_result.error().message);
        co_return;
    }
    
    auto query = relx::select(users.id, users.username)
        .from(users)
        .where(users.id > 10);
    
    // Define a DTO for the query results
    struct UserDTO {
        int id;
        std::string username;
    };
    
    // Execute asynchronously with automatic mapping to UserDTO
    auto result = co_await conn.execute_many<UserDTO>(query);
    if (result) {
        // Process results - automatically mapped to UserDTO objects
        for (const auto& user : *result) {
            std::println("User: {} - {}", user.id, user.username);
        }
    }
    
    // Disconnect
    co_await conn.disconnect();
}

int main() {
    boost::asio::io_context io_context;

    // Start the asynchronous operation
    boost::asio::co_spawn(io_context, run_async_queries(io_context), boost::asio::detached);
    io_context.run();
}
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

## License

[MIT License](LICENSE)
