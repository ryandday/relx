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
    std::cerr << "Connection error: " << conn_result.error().message << std::endl;
    return;
}

// Execute a query
Users users;
auto query = relx::query::select(users.id, users.name).from(users);
auto result = conn.execute(query);

if (!result) {
    // Handle query error
    std::cerr << "Query error: " << result.error().message << std::endl;
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
    std::cerr << "Error: " << error.message << std::endl;
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
    std::cerr << "Error: " << result.error().message << std::endl;
}

// Alternative: Using .error() (only valid if there's an error)
if (!result) {
    auto error = result.error();
    std::cerr << "Error: " << error.message << std::endl;
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
        std::cerr << "Missing required field" << std::endl;
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
        std::cerr << "Connection error: " << conn_result.error().message << std::endl;
        return;
    }
    
    // Execute a query
    auto result = conn.execute(query);
    
    // Force value extraction (could throw)
    const auto& rows = result.value();
    
    // Process rows...
} catch (const std::bad_expected_access<QueryError>& e) {
    std::cerr << "Tried to access result value when there was an error" << std::endl;
} catch (const std::bad_alloc& e) {
    std::cerr << "Out of memory" << std::endl;
} catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << std::endl;
}
```

## Error Propagation

When building larger applications, you'll often want to propagate errors up the call stack:

```cpp
std::expected<std::vector<User>, QueryError> get_active_users(relx::Connection& conn) {
    Users u;
    
    auto query = relx::query::select(u.id, u.name, u.email)
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
    std::cerr << "Error: " << users_result.error().message << std::endl;
    return;
}

for (const auto& user : *users_result) {
    std::cout << "User: " << user.name << std::endl;
}
```

## Debugging and Logging

relx provides several ways to debug and log errors:

### Query String Inspection

You can inspect the generated SQL and parameters:

```cpp
Users u;
auto query = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.age > 18);

// Get the SQL string
std::string sql = query.to_sql();
std::cout << "SQL: " << sql << std::endl;

// Get the parameters
auto params = query.bind_params();
std::cout << "Parameters: ";
for (const auto& param : params) {
    std::cout << param << ", ";
}
std::cout << std::endl;
```

### Error Inspection

Detailed error information is available:

```cpp
auto result = conn.execute(query);
if (!result) {
    auto error = result.error();
    std::cerr << "Error message: " << error.message << std::endl;
    std::cerr << "Error code: " << error.error_code << std::endl;
    std::cerr << "Query: " << error.query_string << std::endl;
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
        std::cerr << "[CONNECTION ERROR] " << error.message 
                  << " (Code: " << error.error_code << ")"
                  << " Source: " << error.source << std::endl;
    }
    
    void log_query_error(const QueryError& error) {
        std::cerr << "[QUERY ERROR] " << error.message 
                  << " (Code: " << error.error_code << ")"
                  << " Query: " << error.query_string << std::endl;
    }
};

// Use the custom logger
ErrorLogger logger;

auto result = conn.execute(query);
if (!result) {
    logger.log_query_error(result.error());
}
``` 