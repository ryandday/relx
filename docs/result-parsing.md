# Result Parsing

relx provides multiple flexible ways to extract and process query results with compile-time type safety. This guide covers the various result parsing methods available in the library.

## Table of Contents

- [Basic Result Processing](#basic-result-processing)
- [DTO Mapping](#dto-mapping)
- [Structured Binding Support](#structured-binding-support)
- [Lazy Result Parsing](#lazy-result-parsing)
- [Column Access Methods](#column-access-methods)
- [Handling Nullable Values](#handling-nullable-values)
- [Error Handling](#error-handling)
- [Best Practices](#best-practices)

## Basic Result Processing

Query execution returns a `ConnectionResult<ResultSet>`, which is a type alias for `std::expected<relx::result::ResultSet, ConnectionError>`.

### Simple Result Iteration

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>

// Define schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "age", int> age;
};

Users users;
auto query = relx::select(users.id, users.name, users.age)
    .from(users)
    .where(users.age > 21);

auto result = conn.execute(query);
if (result) {
    // Iterate over raw result set
    for (const auto& row : *result) {
        // Access individual columns (see other sections)
        auto id = row.get<int>(0);
        auto name = row.get<std::string>(1);
        auto age = row.get<int>(2);
        
        if (id && name && age) {
            std::println("User: {} - {} (Age: {})", *id, *name, *age);
        }
    }
}
```

## DTO Mapping

The most convenient way to process results is through Data Transfer Objects (DTOs) that automatically map query columns to struct members.

### Automatic DTO Mapping

```cpp
// Define a DTO that matches your query structure
struct UserDTO {
    int id;
    std::string name;
    int age;
};

// Execute with automatic mapping
auto result = conn.execute<UserDTO>(query);
if (result) {
    // Iterate over automatically mapped objects
    for (const auto& user : *result) {
        std::println("User: {} - {} (Age: {})", user.id, user.name, user.age);
    }
}
```

**Requirements for DTO Mapping:**
- DTO member names must match column names in the result set
- DTO members must be in the same order as query columns
- DTO types must be compatible with SQL column types

### Complex DTO Examples

```cpp
// DTO with optional fields
struct UserProfileDTO {
    int id;
    std::string name;
    std::string email;
    std::optional<std::string> bio;  // Nullable column
    int age;
};

// DTO for aggregate queries
struct UserStatsDTO {
    std::string department_name;
    int user_count;
    double average_age;
};

// Execute aggregate query with custom DTO
auto stats_query = relx::select_expr(
    departments.name,
    relx::as(relx::count(users.id), "user_count"),
    relx::as(relx::avg(users.age), "average_age")
).from(departments)
 .join(users, relx::on(departments.id == users.department_id))
 .group_by(departments.name);

auto stats_result = conn.execute<UserStatsDTO>(stats_query);
```

## Structured Binding Support

For concise code, use C++17 structured bindings with the `as` helper:

```cpp
auto result = conn.execute(query);
if (result) {
    // Automatic structured binding with type specification
    for (const auto& [id, name, age] : result->as<int, std::string, int>()) {
        std::println("User: {} - {} (Age: {})", id, name, age);
    }
}
```

### Benefits of Structured Binding

- **Type Safety**: Compile-time type checking
- **Concise Code**: No need to define DTOs for simple cases
- **Clear Intent**: Variable names directly in the binding

```cpp
// Multiple binding patterns
for (const auto& [id, name] : result->as<int, std::string>()) {
    process_user(id, name);
}

// Mixed types including optionals
for (const auto& [id, name, bio] : result->as<int, std::string, std::optional<std::string>>()) {
    std::println("User {} ({}): {}", id, name, bio.value_or("No bio"));
}
```

## Lazy Result Parsing

Lazy parsing defers type conversion until data is actually accessed, reducing memory usage and improving performance for partial result processing.

### Basic Lazy Result Set

```cpp
#include <relx/results/lazy_result.hpp>

// Execute query and get lazy result set  
auto result = conn.execute_lazy(query);
if (result) {
    // Iterate over lazy rows - no parsing happens yet
    for (const auto& lazy_row : *result) {
        // Data is parsed only when accessed
        auto id = lazy_row.get<int>("id");
        auto name = lazy_row.get<std::string>("name");
        
        if (id && name) {
            std::println("User {}: {}", *id, *name);
            
            // Only parse expensive columns if needed
            if (*id > 100) {
                auto bio = lazy_row.get<std::string>("bio");
                if (bio) {
                    std::println("  Bio: {}", *bio);
                }
            }
        }
    }
}
```

### Conditional Data Processing

```cpp
// Only parse expensive columns when needed
for (const auto& lazy_row : *result) {
    auto user_type = lazy_row.get<std::string>("user_type");
    
    if (user_type && *user_type == "premium") {
        // Only parse large text fields for premium users
        auto preferences = lazy_row.get<std::string>("preferences");
        auto profile_data = lazy_row.get<std::string>("profile_data");
        
        process_premium_user(preferences.value_or(""), profile_data.value_or(""));
    } else {
        // Basic processing for regular users
        auto name = lazy_row.get<std::string>("name");
        if (name) {
            process_regular_user(*name);
        }
    }
}
```

### Benefits of Lazy Parsing

- **Memory Efficient**: Only parsed data you actually use
- **Performance**: Avoid unnecessary type conversions  
- **Flexibility**: Process different rows differently based on content
- **Streaming Ready**: Works seamlessly with streaming result sets

<div class="alert info">
<strong>💡 Tip:</strong> For large result sets or streaming scenarios, see the comprehensive <a href="streaming-results.html">Streaming Results</a> guide.
</div>

## Column Access Methods

For more control over data access, use explicit column access methods:

### Access by Index

```cpp
const auto& row = result->at(0);  // Get first row

// Type-safe column access by index
auto id = row.get<int>(0);
auto name = row.get<std::string>(1);
auto age = row.get<int>(2);

// Check for errors
if (!id) {
    std::println("Error accessing ID: {}", id.error().message);
} else {
    std::println("User ID: {}", *id);
}
```

### Access by Column Name

```cpp
// Access columns by name (case-sensitive)
auto id = row.get<int>("id");
auto name = row.get<std::string>("name");
auto email = row.get<std::string>("email");

// Handle potential errors
if (name) {
    std::println("User name: {}", *name);
} else {
    std::println("Failed to get name: {}", name.error().message);
}
```

### Access by Schema Column Objects

When you have schema definitions, use column objects directly:

```cpp
Users users;

// Type-safe access using schema column objects
auto id = row.get<int>(users.id);
auto name = row.get<std::string>(users.name);
auto email = row.get<std::string>(users.email);

// This approach provides additional type safety and IDE support
```

## Handling Nullable Values

For columns that might contain NULL values, use `std::optional<T>`:

### Optional Column Access

```cpp
// Schema with nullable column
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "bio", std::optional<std::string>> bio;  // Nullable
};

// Access nullable columns
auto bio = row.get<std::optional<std::string>>("bio");
if (bio) {
    if (*bio) {
        // Bio exists and is not null
        std::println("Bio: {}", **bio);
    } else {
        // Bio column exists but value is null
        std::println("Bio is NULL");
    }
} else {
    // Error accessing bio column
    std::println("Error accessing bio: {}", bio.error().message);
}
```

### DTO with Optional Members

```cpp
struct UserProfileDTO {
    int id;
    std::string name;
    std::optional<std::string> bio;     // Maps to nullable column
    std::optional<int> age;             // Another nullable column
};

auto result = conn.execute<UserProfileDTO>(query);
if (result) {
    for (const auto& user : *result) {
        std::println("User: {}", user.name);
        
        if (user.bio) {
            std::println("  Bio: {}", *user.bio);
        }
        
        if (user.age) {
            std::println("  Age: {}", *user.age);
        }
    }
}
```

## Error Handling

All column access methods return `std::expected` objects that should be checked before use:

### Explicit Error Checking

```cpp
auto name = row.get<std::string>("name");
if (!name) {
    // Handle specific error
    const auto& error = name.error();
    std::println("Error getting name: {}", error.message);
    
    // Potentially retry or use default value
    std::string safe_name = "Unknown";
} else {
    // Use the successful value
    std::println("Name: {}", *name);
}
```

### Exception-Based Error Handling

For convenience, use `value_or_throw` to convert errors to exceptions:

```cpp
try {
    // Extract values or throw on error
    std::string name = relx::value_or_throw(
        row.get<std::string>("name"),
        "Failed to get user name"
    );
    
    int age = relx::value_or_throw(
        row.get<int>("age"),
        "Failed to get user age"
    );
    
    std::println("User: {} ({})", name, age);
    
} catch (const relx::RelxException& e) {
    std::println("Data access error: {}", e.what());
}
```

### Query-Level Error Handling

```cpp
try {
    // Execute query and throw on error
    auto users = relx::value_or_throw(
        conn.execute<UserDTO>(query),
        "Failed to fetch user list"
    );
    
    // Process results knowing they're valid
    std::println("Found {} users", users.size());
    
    for (const auto& user : users) {
        process_user(user);
    }
    
} catch (const relx::RelxException& e) {
    std::println("Query execution failed: {}", e.what());
}
```

## Best Practices

### 1. Use DTOs for Clean Code

```cpp
// Good: Define clear DTOs for complex queries
struct OrderSummaryDTO {
    int order_id;
    std::string customer_name;
    double total_amount;
    std::string status;
    std::optional<std::string> notes;
};

auto orders = relx::value_or_throw(
    conn.execute<OrderSummaryDTO>(complex_order_query)
);
```

### 2. Use Structured Bindings for Simple Cases

```cpp
// Good: Structured bindings for simple data access
for (const auto& [id, name] : result->as<int, std::string>()) {
    simple_processing(id, name);
}
```

### 3. Handle Nullable Columns Explicitly

```cpp
// Good: Explicit handling of nullable data
struct UserDTO {
    int id;
    std::string name;
    std::optional<std::string> phone;  // Clearly nullable
};

for (const auto& user : users) {
    std::println("User: {}", user.name);
    
    if (user.phone) {
        std::println("  Phone: {}", *user.phone);
    } else {
        std::println("  No phone number");
    }
}
```

### 4. Check for Errors Appropriately

```cpp
// Good: Check critical operations
auto critical_data = row.get<std::string>("critical_field");
if (!critical_data) {
    return handle_critical_error(critical_data.error());
}

// Good: Use exceptions for application-level errors
try {
    auto user_data = relx::value_or_throw(
        conn.execute<UserDTO>(query),
        "Failed to load user profile"
    );
    display_user_profile(user_data);
} catch (const relx::RelxException& e) {
    show_user_error_message(e.what());
}
```

### 5. Optimize for Performance

```cpp
// Good: Avoid repeated column access
for (const auto& row : *result) {
    // Access each column once
    auto id = row.get<int>("id");
    auto name = row.get<std::string>("name");
    auto email = row.get<std::string>("email");
    
    if (id && name && email) {
        process_user(*id, *name, *email);
    }
}

// Better: Use DTOs for automatic mapping
auto users = relx::value_or_throw(conn.execute<UserDTO>(query));
for (const auto& user : users) {
    process_user(user.id, user.name, user.email);
}
```

### 6. Match Query Structure to Usage

```cpp
// Good: Query only needed columns
auto minimal_query = relx::select(users.id, users.name)
    .from(users)
    .where(users.active == true);

struct MinimalUserDTO {
    int id;
    std::string name;
};

// Don't over-fetch data
auto users = relx::value_or_throw(
    conn.execute<MinimalUserDTO>(minimal_query)
);
``` 