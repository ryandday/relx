# Error Handling in relx

relx provides robust error handling mechanisms using modern C++23 features like `std::expected`. This document explains how to handle errors in relx applications.

## Table of Contents

- [Error Handling Pattern](#error-handling-pattern)
- [Error Types](#error-types)
- [Working with std::expected](#working-with-stdexpected)
- [Working with Optional Values](#working-with-optional-values)
- [Exception Handling](#exception-handling)
- [Error Propagation](#error-propagation)
- [Debugging and Logging](#debugging-and-logging)

## Error Handling Pattern

relx follows a consistent error handling pattern using `std::expected` for operations that might fail. This approach provides several benefits:

1. Errors are handled explicitly, not through exceptions
2. Error information is preserved and can be inspected
3. The pattern encourages proper error handling

Here's a simple example:

```cpp
// Connect to a database
relx::SQLiteConnection conn("my_database.db");
auto conn_result = conn.connect();

if (!conn_result) {
    // Handle connection error
    std::print("Connection error: {}", conn_result.error().message);
    return;
}

// Execute a query
Users users;
auto query = relx::select(users.id, users.name).from(users);
auto result = conn.execute(query);

if (!result) {
    // Handle query error
    std::print("Query error: {}", result.error().message);
    return;
}

// Process results
for (const auto& row : *result) {
    // Process each row
}
```

## Error Types

relx defines several error types for different operations:

### ConnectionError

Represents errors related to database connections:

```cpp
struct ConnectionError {
    std::string message;
    int error_code;
    std::string source;
};
```

### QueryError

Represents errors in query execution:

```cpp
struct QueryError {
    std::string message;
    int error_code;
    std::string query_string;
};
```

### ResultError

Represents errors when processing query results:

```cpp
struct ResultError {
    std::string message;
};
```

## Working with std::expected

relx uses `std::expected<T, E>` (C++23) for functions that may fail. Here's how to work with it:

### Checking for Success

```cpp
auto result = conn.execute(query);

if (result) {
    // Success case - result contains a value
    // Use *result to access the value
    const auto& rows = *result;
    // Process rows...
} else {
    // Error case - result contains an error
    // Use result.error() to access the error
    auto error = result.error();
    std::print("Error: {}", error.message);
}
```

### Using Value or Error

```cpp
auto result = conn.execute(query);

// Using .value() (will throw std::bad_expected_access if there's an error)
try {
    const auto& rows = result.value();
    // Process rows...
} catch (const std::bad_expected_access<QueryError>& e) {
    std::print("Error: {}", result.error().message);
}

// Alternative: Using .error() (only valid if there's an error)
if (!result) {
    auto error = result.error();
    std::print("Error: {}", error.message);
}
```

### Using and_then for Chaining Operations

```cpp
auto result = conn.execute(query)
    .and_then([](auto&& rows) -> std::expected<std::vector<User>, QueryError> {
        std::vector<User> users;
        // Transform rows to user objects
        for (const auto& row : rows) {
            // ... process row
            users.push_back(User{/*...*/});
        }
        return users;
    });
```

### Using transform for Transforming Values

```cpp
auto result = conn.execute(query)
    .transform([](auto&& rows) {
        std::vector<User> users;
        // Transform rows to user objects
        for (const auto& row : rows) {
            // ... process row
            users.push_back(User{/*...*/});
        }
        return users;
    });
```

### Using or_else for Error Handling

```cpp
auto result = conn.execute(query)
    .or_else([](const QueryError& error) -> std::expected<ResultSet, QueryError> {
        if (error.error_code == SQLITE_BUSY) {
            // Retry the query or return a default result
            return ResultSet{};  // Empty result set
        }
        // Propagate other errors
        return std::unexpected(error);
    });
```

## Working with Optional Values

For columns that may contain NULL values, relx uses `std::optional<T>` and provides a similar error handling pattern:

```cpp
struct User {
    int id;
    std::string name;
    std::optional<std::string> bio;
};

// Execute a query
auto result = conn.execute(query);
if (!result) {
    // Handle error
    return;
}

// Process results
std::vector<User> users;
for (const auto& row : *result) {
    auto id = row.get<int>("id");
    auto name = row.get<std::string>("name");
    auto bio = row.get<std::optional<std::string>>("bio");
    
    if (!id || !name) {
        // Handle missing required fields
        std::print("Missing required field");
        continue;
    }
    
    User user{*id, *name, bio.value_or(std::nullopt)};
    users.push_back(user);
}
```

## Exception Handling

While relx primarily uses `std::expected` for error handling, some operations might still throw exceptions:

1. Out-of-memory conditions
2. Programming errors (invalid arguments, assertions)
3. Calling `.value()` on an `std::expected` that contains an error

Here's how to handle these cases:

```cpp
try {
    // Connect to the database
    relx::SQLiteConnection conn("my_database.db");
    auto conn_result = conn.connect();
    
    if (!conn_result) {
        std::print("Connection error: {}", conn_result.error().message);
        return;
    }
    
    // Execute a query
    auto result = conn.execute(query);
    
    // Force value extraction (could throw)
    const auto& rows = result.value();
    
    // Process rows...
} catch (const std::bad_expected_access<QueryError>& e) {
    std::print("Error: {}", result.error().message);
} catch (const std::bad_alloc& e) {
    std::print("Out of memory");
} catch (const std::exception& e) {
    std::print("Unexpected error: {}", e.what());
}
```

## Error Propagation

When building larger applications, you'll often want to propagate errors up the call stack:

```cpp
std::expected<std::vector<User>, QueryError> get_active_users(relx::Connection& conn) {
    Users u;
    
    auto query = relx::select(u.id, u.name, u.email)
        .from(u)
        .where(u.is_active == true);
    
    auto result = conn.execute(query);
    if (!result) {
        return std::unexpected(result.error());
    }
    
    std::vector<User> users;
    for (const auto& row : *result) {
        auto id = row.get<int>("id");
        auto name = row.get<std::string>("name");
        auto email = row.get<std::string>("email");
        
        if (!id || !name || !email) {
            return std::unexpected(QueryError{
                "Failed to extract user data",
                -1,
                query.to_sql()
            });
        }
        
        users.push_back(User{*id, *name, *email});
    }
    
    return users;
}

// Usage
auto users_result = get_active_users(conn);
if (!users_result) {
    std::print("Error: {}", users_result.error().message);
    return;
}

for (const auto& user : *users_result) {
    std::print("User: {}", user.name);
}
```

## Debugging and Logging

relx provides several ways to debug and log errors:

### Query String Inspection

You can inspect the generated SQL and parameters:

```cpp
Users u;
auto query = relx::select(u.id, u.name)
    .from(u)
    .where(u.age > 18);

// Get the SQL string
std::string sql = query.to_sql();
std::print("SQL: {}", sql);

// Get the parameters
auto params = query.bind_params();
std::print("Parameters: {}", fmt::join(params, ", "));
```

### Error Inspection

Detailed error information is available:

```cpp
auto result = conn.execute(query);
if (!result) {
    auto error = result.error();
    std::print("Error message: {}", error.message);
    std::print("Error code: {}", error.error_code);
    std::print("Query: {}", error.query_string);
}
```

### Custom Error Handling

You can define custom error handlers:

```cpp
// Define a custom error logger
class ErrorLogger {
public:
    void log_connection_error(const ConnectionError& error) {
        // Log to file, send to monitoring system, etc.
        std::print("[CONNECTION ERROR] {} (Code: {} Source: {})", error.message, error.error_code, error.source);
    }
    
    void log_query_error(const QueryError& error) {
        std::print("[QUERY ERROR] {} (Code: {} Query: {})", error.message, error.error_code, error.query_string);
    }
};

// Use the custom logger
ErrorLogger logger;

auto result = conn.execute(query);
if (!result) {
    logger.log_query_error(result.error());
}
``` 