# relx - Modern C++ SQL Query Building Library

Working with SQL often means writing error-prone raw strings. Refactoring is a pain, and in more complex applications you have to write extensive tests to make sure your stringly typed queries work in every situation.

relx is a modern C++23 library designed to solve these problems by constructing and executing SQL queries with compile-time type safety. It provides a fluent, intuitive interface for building SQL queries while preventing SQL injection and type errors. 

Currently features two postgresql clients for database access, an async client (with boost::asio) and a synchronous client.

Once C++26 comes out with reflection (fingers crossed!) then there will be some breaking API changes. But they will make this library better.

## Key Features

- **Type Safety**: SQL queries are validated at compile time
- **Fluent Interface**: Build SQL using intuitive, chainable method calls
- **Schema Definition**: Strongly typed table and column definitions
- **Query Building**: Type-safe SELECT, INSERT, UPDATE, and DELETE operations
- **Boilerplate/Macro Free**: Leverages Boost::pfr for simple schema definitions and automatic result parsing. With C++26 I will be transitioning this to using reflection, to get more compile time safety, remove the boost dependency, and make the API cleaner.

## Requirements

- C++23 compiler support

## Dependencies
- Boost (header-only)
- Google Test (for testing)

### Can use two options for dependencies

- Conan 2.0+ 
- System installed 

## Getting Started

### Building the Project

```bash
# Clone the repository
git clone https://github.com/ryandday/relx.git
cd relx

# A makefile is provided with all the common commands needed to build and test
# This one will run conan, cmake, and make to build the project.
make build

# Run tests - will start a docker postgres container before tests run.
make test
```

### Basic Usage

```cpp
#include <relx/schema.hpp>
#include <relx/postgresql.hpp>  // For PostgreSQLConnection
#include <relx/query.hpp>       // For select, insert_into, create_table, etc.
#include <relx/utility/error_handling.hpp>   // For RelxException
#include <print>                // For std::println (C++23)
#include <string>               // For std::string

// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int> id;
    relx::column<Users, "username", std::string> username;
    relx::column<Users, "email", std::string> email;
    
    // Primary key
    relx::primary_key<&Users::id> pk;
    
    // Unique constraints
    relx::unique_constraint<&Users::email> unique_email;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::column<Posts, "id", int> id;
    relx::column<Posts, "user_id", int> user_id;
    relx::column<Posts, "title", std::string> title;
    relx::column<Posts, "content", std::string> content;
    
    // Primary key
    relx::primary_key<&Posts::id> pk;
    
    // Foreign key
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
        
        conn.connect();

        const Users users;

        // Create users table
        conn.execute(relx::create_table(users).if_not_exists());

        // Insert sample users
        auto insert_statement = relx::insert_into(users)
            .columns(users.username, users.email)
            .values("Jane Smith", "jane@example.com")
            .values("Bob Johnson", "bob@example.com");
        
        conn.execute(insert_statement);

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
        auto rows = conn.execute<UserDTO>(query);

        // Process results - automatically mapped to UserDTO objects
        for (const auto& user : rows) {
            std::println("User: {} - {}", user.id, user.username);
        }
        
        conn.execute(relx::drop_table(users));
        return 0;
    } catch (const relx::RelxException& e) {
        std::cerr << "Query error: " << e.message << std::endl;
        return 1;
    }
}

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

- **Tables**: Define database tables with strongly typed columns
- **Columns**: Specify column names, types, and constraints
- **Primary Keys**: Single-column and composite primary keys
- **Foreign Keys**: Define relationships between tables
- **Indexes**: Create regular and unique indexes
- **Constraints**: Check constraints, unique constraints, default values
- **Nullable Columns**: Support for NULL values via std::optional

## Advanced Features

- **Transaction Support**: RAII-based transaction management
- **Connection Pooling**: Efficient database connection management
- **Result Set Processing**: Strongly typed result access

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
boost::asio::awaitable<void> run_async_queries(auto& connection) {

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

int main {
    boost::asio::io_context io_context;

    // Start the asynchronous operation
    boost::asio::co_spawn(io_context, run_async_queries(), boost::asio::detached);
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
