# Error Handling

relx provides robust error handling mechanisms using C++23's `std::expected`. This guide explains how to handle errors effectively in relx applications.

## Table of Contents

- [Error Handling Philosophy](#error-handling-philosophy)
- [Error Types](#error-types)
- [Working with std::expected](#working-with-stdexpected)
- [Exception Utilities](#exception-utilities)
- [Error Propagation Patterns](#error-propagation-patterns)
- [Debugging and Logging](#debugging-and-logging)
- [Best Practices](#best-practices)

## Error Handling Philosophy

relx follows a modern C++ error handling approach using `std::expected` for operations that might fail. This design provides several key benefits:

- **Explicit Error Handling**: Errors must be handled explicitly, preventing silent failures
- **Type Safety**: Error information is preserved and strongly typed
- **Performance**: No exception overhead for expected failure cases
- **Composability**: Error handling operations can be chained and transformed

### Basic Error Handling Pattern

```cpp
#include <relx/postgresql.hpp>
#include <iostream>

int main() {
    // Connection setup
    relx::PostgreSQLConnectionParams params{
        .host = "localhost",
        .port = 5432,
        .dbname = "mydb",
        .user = "postgres",
        .password = "postgres",
        .application_name = "error_example"
    };
    
    relx::PostgreSQLConnection conn(params);
    
    // Connect with error checking
    auto connect_result = conn.connect();
    if (!connect_result) {
        std::println("Connection failed: {}", connect_result.error().message);
        return 1;
    }
    
    // Execute query with error checking
    Users users;
    auto query = relx::select(users.id, users.name).from(users);
    
    auto query_result = conn.execute(query);
    if (!query_result) {
        std::println("Query failed: {}", query_result.error().message);
        return 1;
    }
    
    // Process successful results
    for (const auto& row : *query_result) {
        // Process each row
    }
    
    return 0;
}
```

## Error Types

relx defines specific error types for different failure categories:

### ConnectionError

Represents database connection-related failures:

```cpp
struct ConnectionError {
    std::string message;        // Human-readable error description
    int error_code;            // Database-specific error code
    std::string source;        // Source of the error (e.g., "PostgreSQL")
    
    // Additional context may be included
    std::optional<std::string> detail;
    std::optional<std::string> hint;
};
```

**Common Connection Errors:**
- Authentication failures
- Network connectivity issues
- Database not found
- Permission denied

### QueryError

Represents SQL query execution failures:

```cpp
struct QueryError {
    std::string message;        // Error description
    int error_code;            // SQL error code
    std::string query_string;  // The SQL that caused the error
    
    // Additional PostgreSQL-specific information
    std::optional<std::string> severity;
    std::optional<std::string> state_code;
    std::optional<int> position;  // Error position in query
};
```

**Common Query Errors:**
- SQL syntax errors
- Constraint violations
- Data type mismatches
- Permission issues

### ResultError

Represents errors when processing query results:

```cpp
struct ResultError {
    std::string message;        // Error description
    std::optional<std::string> column_name;  // Column that caused error
    std::optional<int> row_index;           // Row index if applicable
};
```

**Common Result Errors:**
- Type conversion failures
- Missing columns
- Null value access

## Working with std::expected

All fallible operations in relx return `std::expected<T, ErrorType>` where `T` is the success type and `ErrorType` is the specific error type.

### Checking for Success

```cpp
auto result = conn.execute(query);

// Method 1: Boolean conversion
if (result) {
    // Success - access value with *result
    const auto& rows = *result;
    std::println("Found {} rows", rows.size());
} else {
    // Error - access error with result.error()
    const auto& error = result.error();
    std::println("Query failed: {}", error.message);
}

// Method 2: has_value() check
if (result.has_value()) {
    const auto& rows = result.value();
    // Process rows...
} else {
    const auto& error = result.error();
    // Handle error...
}
```

### Value Access Methods

```cpp
auto result = conn.execute(query);

// Safe access with error checking
if (result) {
    const auto& rows = *result;  // Dereference operator
    // or
    const auto& rows = result.value();  // Explicit value() call
}

// Unsafe access (throws std::bad_expected_access on error)
try {
    const auto& rows = result.value();
    // Process rows...
} catch (const std::bad_expected_access<QueryError>& e) {
    std::println("Unexpected error: {}", result.error().message);
}
```

### Monadic Operations

C++23's `std::expected` supports monadic operations for functional-style error handling:

```cpp
// Transform successful results
auto processed_result = conn.execute(query)
    .transform([](const auto& rows) -> std::vector<UserDTO> {
        std::vector<UserDTO> users;
        for (const auto& row : rows) {
            // Transform rows to DTOs
            users.emplace_back(/* ... */);
        }
        return users;
    });

// Chain operations that might fail
auto chained_result = conn.execute(query)
    .and_then([&conn](const auto& rows) -> relx::ConnectionResult<int> {
        if (rows.empty()) {
            return std::unexpected(QueryError{
                .message = "No rows found",
                .error_code = 0,
                .query_string = "..."
            });
        }
        
        // Perform another operation
        return conn.execute(another_query);
    });

// Handle errors with alternatives
auto with_fallback = conn.execute(query)
    .or_else([&conn](const QueryError& error) -> relx::ConnectionResult<ResultSet> {
        if (error.error_code == SOME_RECOVERABLE_ERROR) {
            // Try a fallback query
            return conn.execute(fallback_query);
        }
        // Propagate unrecoverable errors
        return std::unexpected(error);
    });
```

## Exception Utilities

While `std::expected` is the primary error handling mechanism, relx provides utilities for exception-based error handling when needed.

### throw_if_failed

Converts `std::expected` results to exceptions:

```cpp
#include <relx/error.hpp>

try {
    // These will throw relx::RelxException if they fail
    relx::throw_if_failed(conn.connect());
    relx::throw_if_failed(conn.execute(create_table_query));
    relx::throw_if_failed(conn.execute(insert_query));
    
    std::println("All operations successful");
    
} catch (const relx::RelxException& e) {
    std::println("Operation failed: {}", e.what());
    return 1;
}
```

### value_or_throw

Extracts values from `std::expected` or throws exceptions:

```cpp
try {
    // Extract successful results or throw
    auto users = relx::value_or_throw(
        conn.execute<UserDTO>(query),
        "Failed to fetch users"  // Optional context message
    );
    
    // Process users knowing the operation succeeded
    for (const auto& user : users) {
        std::println("User: {}", user.name);
    }
    
} catch (const relx::RelxException& e) {
    std::println("Error: {}", e.what());
}
```

### Custom Exception Context

Add context to exceptions for better debugging:

```cpp
try {
    auto result = conn.execute<UserDTO>(query);
    auto users = relx::value_or_throw(
        std::move(result),
        "Failed to fetch users from database"
    );
    
    // Process users...
    
} catch (const relx::RelxException& e) {
    std::println("Database operation failed: {}", e.what());
    // Log additional context
    std::println("Query: {}", query.to_sql());
}
```

## Error Propagation Patterns

### Early Return Pattern

```cpp
relx::ConnectionResult<std::vector<UserDTO>> fetch_active_users(
    relx::PostgreSQLConnection& conn
) {
    Users users;
    auto query = relx::select(users.id, users.name, users.email)
        .from(users)
        .where(users.is_active == true);
    
    auto result = conn.execute<UserDTO>(query);
    if (!result) {
        // Propagate error
        return std::unexpected(result.error());
    }
    
    return *result;
}
```

### Error Context Enhancement

```cpp
relx::ConnectionResult<UserStats> calculate_user_stats(
    relx::PostgreSQLConnection& conn,
    int user_id
) {
    auto user_result = fetch_user(conn, user_id);
    if (!user_result) {
        // Enhance error context
        auto error = user_result.error();
        error.message = fmt::format(
            "Failed to calculate stats for user {}: {}",
            user_id, error.message
        );
        return std::unexpected(error);
    }
    
    // Continue with calculations...
    return UserStats{/* ... */};
}
```

## Debugging and Logging

### Error Information Extraction

```cpp
void log_error(const relx::ConnectionError& error) {
    std::println("Connection Error:");
    std::println("  Message: {}", error.message);
    std::println("  Code: {}", error.error_code);
    std::println("  Source: {}", error.source);
    
    if (error.detail) {
        std::println("  Detail: {}", *error.detail);
    }
    if (error.hint) {
        std::println("  Hint: {}", *error.hint);
    }
}

void log_error(const relx::QueryError& error) {
    std::println("Query Error:");
    std::println("  Message: {}", error.message);
    std::println("  Code: {}", error.error_code);
    std::println("  Query: {}", error.query_string);
    
    if (error.position) {
        std::println("  Position: {}", *error.position);
    }
    if (error.state_code) {
        std::println("  State: {}", *error.state_code);
    }
}
```

### Comprehensive Error Handling

```cpp
template<typename T>
void handle_result(const relx::ConnectionResult<T>& result, const std::string& operation) {
    if (!result) {
        const auto& error = result.error();
        
        std::println("Operation '{}' failed:", operation);
        
        // Type-specific error handling
        if constexpr (std::is_same_v<decltype(error), const relx::ConnectionError&>) {
            log_error(error);
        } else if constexpr (std::is_same_v<decltype(error), const relx::QueryError&>) {
            log_error(error);
        } else {
            std::println("  Error: {}", error.message);
        }
    }
}

// Usage
auto result = conn.execute(query);
handle_result(result, "fetch users");
```

## Best Practices

### 1. Always Check Results

```cpp
// Good: Check every result
auto result = conn.execute(query);
if (!result) {
    // Handle error appropriately
    return handle_error(result.error());
}

// Bad: Ignoring potential errors
auto result = conn.execute(query);
// Assuming success without checking
```

### 2. Use Appropriate Error Handling

```cpp
// For libraries: Return std::expected
relx::ConnectionResult<User> fetch_user(int id) {
    auto result = conn.execute<UserDTO>(query);
    if (!result) {
        return std::unexpected(result.error());
    }
    return convert_to_user(*result);
}

// For applications: Use exceptions when appropriate
void application_logic() {
    try {
        auto user = relx::value_or_throw(fetch_user(123));
        process_user(user);
    } catch (const relx::RelxException& e) {
        show_user_error(e.what());
    }
}
```

### 3. Provide Context

```cpp
// Good: Add meaningful context
auto users = relx::value_or_throw(
    conn.execute<UserDTO>(query),
    "Failed to load user list for dashboard"
);

// Better: Include relevant parameters
auto users = relx::value_or_throw(
    conn.execute<UserDTO>(query),
    fmt::format("Failed to load users for department {}", dept_id)
);
```

### 4. Handle Specific Error Types

```cpp
auto result = conn.execute(query);
if (!result) {
    const auto& error = result.error();
    
    switch (error.error_code) {
        case UNIQUE_VIOLATION:
            return handle_duplicate_user();
        case FOREIGN_KEY_VIOLATION:
            return handle_invalid_reference();
        default:
            return handle_general_error(error);
    }
}
```

### 5. Clean Resource Management

```cpp
relx::ConnectionResult<void> perform_transaction() {
    auto transaction = conn.begin_transaction();
    
    try {
        relx::throw_if_failed(conn.execute(query1));
        relx::throw_if_failed(conn.execute(query2));
        relx::throw_if_failed(transaction.commit());
        
        return {}; // Success
        
    } catch (const relx::RelxException& e) {
        auto rollback_result = transaction.rollback();
        if (!rollback_result) {
            // Log rollback failure but return original error
            std::println("Rollback failed: {}", rollback_result.error().message);
        }
        
        return std::unexpected(QueryError{
            .message = e.what(),
            .error_code = 0,
            .query_string = "transaction"
        });
    }
}
```