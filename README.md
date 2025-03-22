# SQLlib - Modern C++ SQL Query Building Library

SQLlib is a modern C++23 library for constructing and executing SQL queries with compile-time type safety. It provides a fluent, intuitive interface for building SQL queries while preventing SQL injection and type errors.

## Key Features

- **Type Safety**: SQL queries are validated at compile time
- **Fluent Interface**: Build SQL using intuitive, chainable method calls
- **Schema Definition**: Strongly typed table and column definitions
- **Query Building**: Type-safe SELECT, INSERT, UPDATE, and DELETE operations
- **Compile-time Validation**: Catch SQL syntax errors during compilation when possible
- **Modern C++ Design**: Leverages C++23 features like concepts, compile-time evaluation, and more

## Requirements

- C++23 compiler support
  - GCC 11+
  - Clang 14+
  - MSVC 19.29+ (Visual Studio 2022+)
- CMake 3.15+
- Conan 2.0+ (for dependencies)

## Dependencies

- Boost (header-only)
- fmt
- Google Test (for testing)

## Getting Started

### Building the Project

#### Install conan

```bash
# Clone the repository
git clone https://github.com/yourusername/sqllib.git
cd sqllib

# Build using make
make build

# Run tests
make test
```

### Basic Usage

```cpp
#include <sqllib/schema.hpp>
#include <iostream>

// Define a schema
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    
    // Primary key
    sqllib::schema::primary_key<&Users::id> pk;
    
    // Unique constraints
    sqllib::schema::unique_constraint<&Users::email> unique_email;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    
    // Primary key
    sqllib::schema::primary_key<&Posts::id> pk;
    
    // Foreign key
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

// Full query building and execution examples coming soon!
```

## Schema Features

SQLlib provides a rich set of schema definition features:

- **Tables**: Define database tables with strongly typed columns
- **Columns**: Specify column names, types, and constraints
- **Primary Keys**: Single-column and composite primary keys
- **Foreign Keys**: Define relationships between tables
- **Indexes**: Create regular and unique indexes
- **Constraints**: Check constraints, unique constraints, default values
- **Nullable Columns**: Support for NULL values via std::optional

## Advanced Features

- **Compile-time SQL Generation**: SQL strings generated at compile time where possible
- **Type-safe Parameter Binding**: Automatic parameter binding with correct types
- **Transaction Support**: RAII-based transaction management
- **Connection Pooling**: Efficient database connection management
- **Result Set Processing**: Strongly typed result access

## Documentation

For more detailed documentation, check the following:

- [Schema Example](examples/schema_example.cpp): Complete example of schema definition
- [DB Connection API](DB_CONNECTION_API.md): Database connection API documentation
- [Coding Standards](CODING_STANDARDS.md): C++ coding standards for this project

## Project Structure

- `include/sqllib/`: Header files
- `src/`: Implementation files
- `examples/`: Usage examples
- `tests/`: Unit tests
- `build/`: Build output (created during build)

## Contributing

Contributions are welcome! Please follow our [Coding Standards](CODING_STANDARDS.md) when making changes.

## License

[MIT License](LICENSE)
