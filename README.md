# relx

[![CI](https://github.com/ryandday/relx/workflows/CI/badge.svg)](https://github.com/ryandday/relx/actions)
[![codecov](https://codecov.io/gh/ryandday/relx/branch/main/graph/badge.svg)](https://codecov.io/gh/ryandday/relx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-supported-336791.svg)](https://www.postgresql.org/)

## Modern C++ SQL Query Building Library

> 🚀 **Transform your SQL experience** - Build type-safe, compile-time validated SQL queries with modern C++23

Working with SQL often means writing error prone raw strings. Refactoring is a pain, and in more complex applications you have to write extensive tests to make sure your stringly typed queries work in every situation.

relx is a modern C++23 library designed to solve these problems by constructing and executing SQL queries with compile-time type safety. It provides a fluent, intuitive interface for building SQL queries while preventing SQL injection and type errors. 

The core library is header-only. relx also features two PostgreSQL clients for database access: an async client (with `boost::asio`) and a synchronous client. The PostgreSQL clients are not header-only and require building with [libpq](https://www.postgresql.org/docs/current/libpq.html).

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
| **Boilerplate/Macro Free** | Leverages Boost::pfr for simple schema definitions | Clean, readable code without preprocessor magic |
| **Header-Only Core** | Core library requires no compilation | Easy integration, fast build times |

> **Future-Ready**: With C++26 reflection coming (fingers crossed!), we'll transition from Boost::pfr to standard reflection for even better compile-time safety and cleaner APIs.

## Documentation

- **[User Guides](docs/)** - Comprehensive documentation and examples
- **[API Documentation](https://ryandday.github.io/relx/)** - Auto-generated from source code

## Quick Start

### Requirements

- C++23 bleeding edge compiler (GCC 15, Clang 20)

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

// Define schemas
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "username", std::string> username;
    relx::column<Users, "email", std::string> email;
    
    relx::unique_constraint<&Users::email> unique_email;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::column<Posts, "id", int, relx::primary_key> id;
    relx::column<Posts, "user_id", int> user_id;
    relx::column<Posts, "title", std::string> title;
    relx::column<Posts, "content", std::string> content;
    
    relx::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

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

        const Users users;

        auto create_table_result = conn.execute(relx::create_table(users).if_not_exists());
        relx::throw_if_failed(create_table_result);

        auto insert_statement = relx::insert_into(users)
            .columns(users.username, users.email)
            .values("Jane Smith", "jane@example.com")
            .values("Bob Johnson", "bob@example.com");
        
        auto insert_result = conn.execute(insert_statement);
        relx::throw_if_failed(insert_result);

        // Building a simple SELECT query
        auto query = relx::select(users.id, users.username)
            .from(users)
            .where(users.id > 10);
        
        // Generate the SQL
        std::string sql = query.to_sql();
        // Output: "SELECT id, username FROM users WHERE (id > ?)"
        
        // Get the bind parameters
        auto params = query.bind_params();
        // params[0] = "10"
        
        // Define a DTO to receive the results
        struct UserDTO {
            int id;
            std::string username;
        };
        
        // Execute the query
        auto query_result = conn.execute<UserDTO>(query);
        const auto& rows = relx::value_or_throw(query_result);

        // Process results - automatically mapped to UserDTO objects
        for (const auto& user : rows) {
            std::println("User: {} - {}", user.id, user.username);
        }
        
        auto drop_result = conn.execute(relx::drop_table(users));
        relx::throw_if_failed(drop_result);
        return 0;
    } catch (const relx::RelxException& e) {
        std::println("{}", e.what());
        return 1;
    }
}
```

### Creating and Dropping Table Examples

```cpp
// Define schemas (as shown in previous examples)
Users users;
Posts posts;

// Create tables
auto create_users = relx::schema::create_table(users);
auto create_posts = relx::schema::create_table(posts).if_not_exists();

// Drop tables
auto drop_users = relx::drop_table(users).if_exists().cascade();
auto drop_posts = relx::drop_table(posts).if_exists();
```

### Query Building Examples

#### SELECT Queries

```cpp
Users users;
Posts posts;

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
Users users;

// Simple INSERT
auto insert1 = relx::insert_into(users)
    .values(
        relx::set(users.username, "john_doe"),
        relx::set(users.email, "john@example.com")
    );

// Multi-row INSERT
auto insert2 = relx::insert_into(users)
    .columns(users.username, users.email)
    .values("john_doe", "john@example.com")
    .values("jane_smith", "jane@example.com");
```

#### UPDATE Queries

```cpp
Users users;

// Basic UPDATE
auto update1 = relx::update(users)
    .set(
        relx::set(users.username, "new_username"),
        relx::set(users.email, "new_email@example.com")
    )
    .where(users.id == 1);
```

#### DELETE Queries

```cpp
Users users;

// Basic DELETE
auto delete1 = relx::delete_from(users)
    .where(users.id == 1);

// DELETE all
auto delete2 = relx::delete_from(users);
```

For more advanced examples, see our [documentation](docs/README.md).

## Schema Features

relx provides a rich set of schema definition features:

| Feature | Description | Example |
|---------|-------------|---------|
| **Tables** | Define database tables with strongly typed columns | `struct Users { static constexpr auto table_name = "users"; ... };` |
| **Columns** | Specify column names, types, and constraints | `relx::column<Users, "id", int, relx::primary_key> id;` |
| **Primary Keys** | Single-column and composite primary keys | `relx::primary_key<&Users::id>` |
| **Foreign Keys** | Define relationships between tables | `relx::foreign_key<&Posts::user_id, &Users::id>` |
| **Indexes** | Create regular and unique indexes | `relx::unique_constraint<&Users::email>` |
| **Constraints** | Check constraints, unique constraints, default values | `relx::check_constraint</* condition */>` |
| **Nullable Columns** | Support for NULL values via std::optional | `relx::column<Users, "age", std::optional<int>>` |

## Advanced Features

- **Transaction Support**: RAII-based transaction management
- **Connection Pooling**: Efficient database connection management  
- **Result Set Processing**: Strongly typed result access
- **Async Operations**: Non-blocking I/O with Boost.Asio
- **Type Safety**: Compile-time SQL validation

## Supported Databases

### PostgreSQL

relx provides robust support for PostgreSQL through several connection types to meet different requirements:

#### Standard Connection

The `PostgreSQLConnection` class provides a synchronous interface for PostgreSQL:

```cpp
#include <relx/postgresql.hpp>

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

// Define the schema
struct Users {
    static constexpr auto table_name = "users";
    relx::column<Users, "id", int> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
};

// Create a table instance
Users users;

// Define a DTO for user data
struct UserDTO {
    std::string name;
};

// Build a type-safe query using the fluent API
auto query = relx::select(users.name)
    .from(users)
    .where(users.id > 5);

// Execute the query with automatic mapping to UserDTO
auto result = conn.execute<UserDTO>(query);
if (result) {
    // Process results - automatically mapped to UserDTO objects
    for (const auto& user : *result) {
        std::println("Name: {}", user.name);
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

// Method 1: Get a connection from the pool
auto conn_result = pool->get_connection();
if (conn_result) {
    auto& conn = *conn_result;
    
    // Define schema
    Users users;
    
    // Create a type-safe query
    auto query = relx::select_expr(relx::as(relx::count_all(), "count")).from(users);
    
    // Define a DTO for the count result
    struct CountDTO {
        int count;
    };
    
    // Execute with automatic mapping to CountDTO
    auto query_result = conn->execute<CountDTO>(query);
    
    if (query_result) {
        std::println("User count: {}", (*query_result).at(0).count);
    }
    
    // Connection automatically returned to pool when conn goes out of scope
}

// Method 2: Use the with_connection pattern
auto user_count = pool->with_connection([](auto& conn) -> relx::ConnectionResult<int> {
    // Define a simple DTO for the count result
    struct CountDTO {
        int count;
    };
    
    Users users;
    auto query = relx::select_expr(relx::as(relx::count_all(), "count")).from(users);
    auto result = conn->execute<CountDTO>(query);
    
    if (!result) {
        return std::unexpected(result.error());
    }
    
    return (*result).at(0).count;
});

if (user_count) {
    std::println("User count: {}", *user_count);
}
```

#### Asynchronous Connection

For non-blocking I/O, use the `PostgreSQLAsyncConnection` with Boost.Asio:

```cpp
#include <relx/postgresql.hpp>
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
    
    // Define schema
    Users users;
    
    // Simple query
    auto query = relx::select(users.id, users.username)
        .from(users)
        .where(users.id > 10);
    
    // Define a DTO for the query results
    struct UserDTO {
        int id;
        std::string username;
    };
    
    // Execute asynchronously with automatic mapping to UserDTO
    auto result = co_await conn.execute<UserDTO>(query);
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
