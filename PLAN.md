# Type-Safe SQL Library (SQLlib) Plan

## Overview
SQLlib will be a modern C++ library for constructing and executing SQL queries with compile-time type safety. The library will use method chaining to build queries in a fluent, intuitive style while preventing SQL injection and type errors.

## Core Design Goals
1. **Type Safety**: Ensure SQL queries are validated at compile time
2. **Fluent Interface**: Create SQL using intuitive, chainable method calls
3. **Composability**: Allow queries and query parts to be composed and reused
4. **Minimal Runtime Overhead**: Optimize for minimal performance impact
5. **Compile-time Validation**: Catch SQL syntax errors at compile time when possible

## Architecture

### Core Components

#### 1. Schema Definition
- Strongly typed table and column definitions
- Type-safe foreign key relationships
- Support for common SQL data types with C++ mapping

#### 2. Query Building
- SELECT query builder
- INSERT query builder
- UPDATE query builder
- DELETE query builder
- Specialized JOIN expressions

#### 3. Expression System
- Type-safe SQL expressions
- Comparison operators (=, !=, >, <, LIKE, etc.)
- Logical operators (AND, OR, NOT)
- Mathematical operators (+, -, *, /)
- Aggregate functions (SUM, AVG, COUNT, etc.)

#### 4. Database Connection
- Connection management
- Transaction support
- Prepared statement management

#### 5. Result Processing
- Strongly typed result sets
- Row iteration
- Data extraction with type conversion

## Implementation Strategy

### Phase 1: Core Expression System
- Implement basic expression templates
- Build type-safe column references
- Create the foundation for SQL operation building

### Phase 2: Basic Query Building
- Implement SELECT query building
- Add WHERE clause support
- Build basic JOIN functionality
- Create type-safe result processing

### Phase 3: Complete SQL Support
- Implement INSERT, UPDATE, DELETE operations
- Add transaction support
- Support for subqueries
- Add aggregate functions and GROUP BY

### Phase 4: Advanced Features
- Schema migration tools
- Query optimization hints
- Bulk operations
- Connection pooling

## Schema Definition Improvements

### Planned Enhancements
1. ✅ **Nullable Types with std::optional Integration**
   - Replace the current nullable_column implementation with std::optional integration
   - Add specialized column_traits for std::optional types
   - Ensure proper NULL handling in SQL generation
   - Example:
   ```cpp
   column<"email", std::optional<std::string>> email; // Automatically NULL-able
   ```

2. ✅ **Check Constraints**
   - Add support for SQL CHECK constraints on columns or tables
   - Provide a compile-time friendly way to define constraints
   - Support both simple and complex conditions
   - Example:
   ```cpp
   check_constraint<&price, "price > 0"> price_positive;
   check_constraint<"age >= 18 AND country = 'USA'"> adult_us_check;
   ```

3. ✅ **Default Values**
   - Add support for DEFAULT value constraints on columns
   - Include common defaults (CURRENT_TIMESTAMP, etc.)
   - Support both literal and expression defaults
   - Example:
   ```cpp
   column<"created_at", timestamp, DefaultValue<"CURRENT_TIMESTAMP">> created_at;
   column<"status", std::string, DefaultValue<"'active'">> status;
   ```

4. ✅ **Unique Constraints**
   - Add dedicated unique constraint support separate from unique indexes
   - Support both single column and multi-column unique constraints
   - Example:
   ```cpp
   unique_constraint<&user::email> unique_email;
   unique_constraint<&user::first_name, &user::last_name> unique_name_pair;
   ```

5. **Composite Foreign Keys**
   - Implement composite foreign keys for relationships with tables having composite primary keys
   - Support multiple columns for both local and referenced key parts
   - Support all reference actions (CASCADE, SET NULL, etc.)
   - Example:
   ```cpp
   composite_foreign_key<&Order::customer_id, &Order::order_date, 
                         &Customer::id, &Customer::registration_date> order_customer_fk;
   ```

6. **Uniform Constraint Interface**
   - Create consistent naming and interfaces across all constraint types
   - Provide unified template aliases like the existing `unique<...>` for primary keys and check constraints
   - Ensure consistent parameter ordering and defaults
   - Example:
   ```cpp
   pk<&Table::id>                    // For primary key
   unique<&Table::email>             // For unique constraint (already exists)
   check<"column > 0">               // For check constraint (already exists but needs standardization)
   fk<&Table::local, &Other::remote> // For foreign key
   ```

7. **Explicit Column Check Constraint Association**
   - Add explicit column binding for single-column check constraints
   - Maintain the current string-based approach for table-level constraints
   - Improve error detection for invalid column references
   - Example:
   ```cpp
   // Explicitly column-bound check constraint
   column_check<&Table::price, "price > 0"> price_positive;
   
   // Table-level constraint remains the same
   check<"total_price = sum(price * quantity)"> total_price_check;
   ```

## Usage Examples

### Schema Definition
```cpp
namespace sqllib {

// Define a schema
struct User {
    static constexpr auto name = std::string_view("users");
    
    column<"id", int> id;
    column<"name", std::string> name;
    column<"email", std::string> email;
    column<"created_at", std::chrono::system_clock::time_point> created_at;
    
    primary_key<&User::id> pk;
    sqllib::schema::index<&User::email> email_idx;
};

struct Post {
    static constexpr auto name = std::string_view("posts");
    
    column<"id", int> id;
    column<"user_id", int> user_id;
    column<"title", std::string> title;
    column<"content", std::string> content;
    
    primary_key<&Post::id> pk;
    foreign_key<&Post::user_id, &User::id> user_fk;
};

} // namespace sqllib
```

### Query Building
```cpp
namespace sqllib {

// Creating and executing a query
User u;
Post p;

auto query = select(u.name, u.email, p.title)
    .from(u)
    .join(p, on(u.id == p.user_id))
    .where(u.id == 1 && p.title.like("%important%"))
    .order_by(p.created_at.desc());

auto results = connection.execute(query);

// Processing results
for (const auto& row : results) {
    std::string name = row.get<0>();  // Type-safe access
    std::string email = row.get<1>();
    std::string title = row.get<2>();
    
    // Process row...
}

} // namespace sqllib
```

### Enhanced Result Processing
```cpp
namespace sqllib {

// More type-safe result processing

// 1. Member pointer access for compile-time type safety
for (const auto& row : results) {
    std::string name = row.get<&User::name>();     // Access by member pointer
    std::string email = row.get<&User::email>();   // Type and column checked at compile time
    std::string title = row.get<&Post::title>();   // Error if column wasn't in the SELECT
    
    // Process row...
}

// 2. Structured binding support with result traits
for (const auto& [name, email, title] : results.as<std::string, std::string, std::string>()) {
    // Types checked against column types
    // Process with directly bound variables...
}

// 3. Row object conversion with static reflection
struct UserPost {
    std::string name;
    std::string email;
    std::string title;
};

// Map fields using member pointers at compile time
auto user_posts = results.transform<UserPost>({
    {&UserPost::name, &User::name},
    {&UserPost::email, &User::email},
    {&UserPost::title, &Post::title}
});

for (const auto& user_post : user_posts) {
    // Use strongly typed object
    std::cout << user_post.name << " wrote: " << user_post.title << std::endl;
}

// 4. Direct transformation with lambdas
auto processed_results = results.transform([](const auto& row) {
    return std::make_tuple(
        row.get<&User::name>(),
        row.get<&User::email>(),
        row.get<&Post::title>()
    );
});

// 5. Optional handling for possibly NULL values
auto maybe_email = row.get_optional<&User::email>();  // Returns std::optional<std::string>
if (maybe_email) {
    send_email(*maybe_email);
}

} // namespace sqllib
```

### Enhanced Schema Definition (With Implemented Features)
```cpp
namespace sqllib {

// Enhanced schema definition with new features
struct Product {
    static constexpr auto name = std::string_view("products");
    
    column<"id", int> id;
    column<"name", std::string> name;
    column<"price", double> price;
    column<"category", std::string> category;
    column<"description", std::optional<std::string>> description; // Nullable with std::optional
    column<"stock", int, DefaultValue<0>> stock;                 // Default value with integer literal
    column<"created_at", timestamp, DefaultValue<"CURRENT_TIMESTAMP">> created_at;
    column<"status", std::string, DefaultValue<"'active'">> status;
    
    primary_key<&Product::id> pk;
    unique_constraint<&Product::name> unique_name;               // Unique constraint
    check_constraint<&Product::price, "price >= 0"> valid_price; // Check constraint
    
    // Complex check constraint for the entire table
    check_constraint<nullptr, "stock >= 0 AND (status = 'active' OR status = 'discontinued')"> valid_product;
    
    // Composite unique constraint
    composite_unique_constraint<&Product::category, &Product::name> unique_category_name;
    
    // Create an index on category for faster lookups
    sqllib::schema::index<&Product::category> category_idx;
};

struct Order {
    static constexpr auto name = std::string_view("orders");
    
    column<"id", int> id;
    column<"product_id", int> product_id;
    column<"quantity", int, DefaultValue<1>> quantity;         // Integer literal for default value
    column<"user_id", std::optional<int>> user_id;              // Optional user association
    column<"order_date", timestamp, DefaultValue<"CURRENT_TIMESTAMP">> order_date;
    
    primary_key<&Order::id> pk;
    foreign_key<&Order::product_id, &Product::id> product_fk;
    check_constraint<&Order::quantity, "quantity > 0"> valid_quantity;
};

} // namespace sqllib
```

### Query Building
```cpp
namespace sqllib {

// Creating and executing a query
User u;
Post p;

auto query = select(u.name, u.email, p.title)
    .from(u)
    .join(p, on(u.id == p.user_id))
    .where(u.id == 1 && p.title.like("%important%"))
    .order_by(p.created_at.desc());

auto results = connection.execute(query);

// Processing results
for (const auto& row : results) {
    std::string name = row.get<0>();  // Type-safe access
    std::string email = row.get<1>();
    std::string title = row.get<2>();
    
    // Process row...
}

} // namespace sqllib
```

## Dependencies
- Standard C++ library
- (Optional) SQLite, MySQL, or PostgreSQL client libraries
- (Optional) Boost for advanced template metaprogramming

## Testing Strategy
- Unit tests for expression building
- Integration tests with SQLite in-memory database
- End-to-end tests with actual database engines
- Performance benchmarks against raw SQL

## Timeline
1. **Week 1-2**: Core expression system and type system
2. **Week 3-4**: Basic SELECT query builder
3. **Week 5-6**: Complete SELECT queries with JOIN support
4. **Week 7-8**: Implement other query types (INSERT, UPDATE, DELETE)
5. **Week 9-10**: Database connection management and result processing
6. **Week 11-12**: Advanced features and optimization