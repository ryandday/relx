# Result Parsing in relx

relx provides multiple flexible ways to extract and process query results with compile-time type safety. This document covers the various result parsing methods available in the library.

## Basic Result Parsing

The most common way to get results in relx is by executing a query through a database connection:

```cpp
auto result = conn.execute(query);
```

The result is returned as a `ConnectionResult<ResultSet>` which is a type alias for `std::expected<relx::result::ResultSet, ConnectionError>`.

## DTO Mapping and Iteration

The simplest way to work with query results is to use a Data Transfer Object (DTO) that matches your query structure and iterate over the results:

```cpp
// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<Users, "id", int> id;
    relx::schema::column<Users, "name", std::string> name;
    relx::schema::column<Users, "email", std::string> email;
    relx::schema::column<Users, "age", int> age;
};

// Define a DTO that matches your query fields
struct UserDTO {
    int id;
    std::string name;
    int age;
};

Users users;
auto query = relx::query::select(users.id, users.name, users.age)
    .from(users)
    .where(users.age > 21);

// Execute with automatic mapping to the DTO
auto result = conn.execute<UserDTO>(query);
if (result) {
    // Iterate over the mapped objects
    for (const auto& user : *result) {
        std::cout << "User: " << user.id << " - " << user.name 
                  << " (Age: " << user.age << ")" << std::endl;
    }
}
```

When you use DTO mapping, relx automatically maps the result columns to the DTO's member variables based on their names. The column names must match the member names of your DTO and must be in the right order.

You can also iterate over the raw result set without using DTOs:

```cpp
auto result = conn.execute(query);
if (result) {
    for (const auto& row : *result) {
        // Process each row (see other sections for accessing column data)
    }
}
```

## Structured Binding Support

For more concise code, you can use C++17 structured bindings with query results:

```cpp
// Using the "as" helper for automatic binding
for (const auto& [id, name, age] : results.as<int, std::string, int>()) {
    // Now id, name, and age are typed and bound to the corresponding columns
    std::cout << id << ": " << name << " (" << age << ")" << std::endl;
}
```

The `as<T1, T2, T3, ...>()` helper automatically binds the columns in the result set to variables with the corresponding types. The columns are bound in the order they appear in the result set. This is the simplest way to use structured bindings with relx query results.

## Accessing by Index

For direct access to column values, you can use indices:

```cpp
// Get the first row
const auto& row = results.at(0);

// Get column values by index
auto id = row.get<int>(0);
auto name = row.get<std::string>(1);

// Handle errors
if (!id) {
    std::cerr << "Error: " << id.error().message << std::endl;
} else {
    std::cout << "ID: " << *id << std::endl;
}
```

## Accessing by Column Name or Object

You can access columns by name:

```cpp
// Get values by column name
auto id = row.get<int>("id");
auto name = row.get<std::string>("name");
```

When you have a schema defined, you can also use column objects directly:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<Users, "id", int> id;
    relx::schema::column<Users, "name", std::string> name;
    // other columns...
};

Users users;

// Get values using column objects
auto id = row.get<int>(users.id);
auto name = row.get<std::string>(users.name);
```

## Optional Values

For columns that might be NULL, you can use std::optional:

```cpp
// Get a nullable column value
auto email = row.get<std::optional<std::string>>("email");
```

This allows you to handle NULL values gracefully in your code:

```cpp
if (email && *email) {
    // Email exists and is not null
    std::cout << "Email: " << **email << std::endl;
} else if (email) {
    // Email exists but is null
    std::cout << "Email is NULL" << std::endl;
} else {
    // Error accessing email
    std::cerr << "Error: " << email.error().message << std::endl;
}
```

## Error Handling

All extraction methods return `std::expected` objects that you should check before using:

```cpp
auto name = row.get<std::string>("name");
if (!name) {
    // Handle error
    std::cerr << "Error getting name: " << name.error().message << std::endl;
} else {
    // Use the value
    std::cout << "Name: " << *name << std::endl;
}
```

For convenience, relx provides `value_or_throw` to automatically throw an exception if an error occurs:

```cpp
try {
    // This will throw an exception if the result contains an error
    std::string name = relx::value_or_throw(row.get<std::string>("name"));
    std::cout << "Name: " << name << std::endl;
} catch (const relx::ErrorException& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
}
```

For query execution results:

```cpp
try {
    // Execute and throw on error
    auto results = relx::value_or_throw(conn.execute(query));
    
    // Process results knowing they're valid
    std::cout << "Found " << results.size() << " rows" << std::endl;
} catch (const relx::ErrorException& ex) {
    std::cerr << "Query error: " << ex.what() << std::endl;
}
```

## Best Practices

1. **Use DTOs for Clean Code**: Whenever possible, define DTOs that match your query structure for automatic mapping.

2. **Prefer Structured Bindings**: For concise code, use structured bindings with the `as` helper.

3. **Handle Errors**: Always check if the result is valid before using it, especially when accessing nullable columns.

4. **Use std::optional**: For columns that might be NULL, always use std::optional to avoid runtime errors.

5. **Consider Performance**: For large result sets, prefer iterating directly over rows rather than processing the entire set upfront.

## Low-Level Result Parsing

For advanced use cases or testing, relx provides a lower-level function to directly parse result strings:

```cpp
auto result = relx::result::parse(query, raw_results_string);
```

This function takes a query object and a raw result string (typically the output from a database driver) and parses it into a `relx::result::ResultSet`. The result is returned as a `relx::result::ResultProcessingResult<relx::result::ResultSet>` which is a type alias for `std::expected<relx::result::ResultSet, relx::result::ResultError>`.

This is primarily useful for:
- Testing result handling code without a database connection
- Custom database drivers that need to integrate with relx
- Processing raw database output from other sources 