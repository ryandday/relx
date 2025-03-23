# Learn SQL with SQLlib: A Beginner's Guide

## Table of Contents

1. **Introduction to SQL and SQLlib**
   - What is SQL?
   - Why Learn SQL?
   - Introduction to SQLlib
   - Setting Up Your Environment

2. **Understanding Database Fundamentals**
   - What is a Database?
   - Relational Database Concepts
   - Tables, Rows, and Columns
   - Primary Keys and Foreign Keys
   - Data Types in SQL

3. **Defining Database Schema with SQLlib**
   - Creating Your First Table Schema
   - Defining Columns and Data Types
   - Working with Primary Keys
   - Creating Foreign Key Relationships
   - Adding Constraints and Indexes
   - Nullable Fields with std::optional

4. **Basic SQL Queries with SQLlib**
   - Your First SELECT Query
   - Understanding Query Building
   - Filtering Data with WHERE Clauses
   - Sorting Data with ORDER BY
   - Limiting Results

5. **Working with Multiple Tables**
   - Understanding Table Relationships
   - Introduction to JOINs
   - INNER JOINs with SQLlib
   - LEFT and RIGHT JOINs
   - Using Foreign Keys in Queries

6. **Data Manipulation with SQLlib**
   - Inserting Data with INSERT
   - Updating Data with UPDATE
   - Deleting Data with DELETE
   - Transaction Management

7. **Advanced Queries**
   - Aggregate Functions (COUNT, SUM, AVG, etc.)
   - GROUP BY and HAVING Clauses
   - Subqueries
   - CASE Expressions
   - Working with NULLs

8. **SQL Functions and Expressions**
   - String Functions
   - Date/Time Functions
   - Mathematical Operations
   - Conditional Logic

9. **Performance Optimization**
   - Understanding Indexes
   - Query Optimization Techniques
   - Using EXPLAIN to Analyze Queries
   - Best Practices for Performance

10. **Real-World Examples**
    - Building a Blog Application Schema
    - Implementing Common Query Patterns
    - Solving Practical Database Problems
    - End-to-End Application Example

11. **Advanced SQLlib Features**
    - Type-Safe Queries
    - Compile-Time Validation
    - Working with Result Sets
    - Error Handling

12. **SQL Security and Best Practices**
    - Preventing SQL Injection
    - User Authentication and Authorization
    - Backup and Recovery
    - Database Design Best Practices

## 1. Introduction to SQL and SQLlib

### What is SQL?

SQL (Structured Query Language) is a domain-specific language used for managing and manipulating relational databases. First developed in the 1970s by IBM researchers, SQL has become the standard language for interacting with relational database management systems (RDBMS).

SQL allows you to:
- Create, modify, and delete database objects (tables, indexes, etc.)
- Insert, update, and delete data in your database
- Query and retrieve data from your database
- Control access to your database objects and data

SQL follows a declarative programming paradigm, which means you specify WHAT you want to accomplish rather than HOW to accomplish it. The database engine determines the most efficient way to execute your query.

### Why Learn SQL?

In today's data-driven world, SQL skills are more valuable than ever:

1. **Data is Everywhere**: Almost every application, from mobile apps to enterprise software, relies on databases.

2. **Career Opportunities**: SQL is consistently ranked among the most in-demand technical skills across industries.

3. **Data Analysis**: SQL is essential for data analysis, business intelligence, and making data-driven decisions.

4. **Problem Solving**: SQL enables you to answer complex questions about your data efficiently.

5. **Universal Language**: SQL is standardized and used across virtually all relational database systems (MySQL, PostgreSQL, SQLite, Oracle, SQL Server, etc.), making it a transferable skill.

### Introduction to SQLlib

SQLlib is a modern C++23 library that provides a type-safe, intuitive interface for constructing and executing SQL queries. Unlike traditional approaches where SQL queries are written as raw strings, SQLlib allows you to build queries using C++ code, leveraging the compiler to catch errors before runtime.

Key features of SQLlib include:

- **Type Safety**: SQL queries are validated at compile time, preventing many common errors.
- **Fluent Interface**: Build SQL using intuitive, chainable method calls.
- **Schema Definition**: Create strongly typed table and column definitions.
- **Query Building**: Type-safe SELECT, INSERT, UPDATE, and DELETE operations.
- **Compile-time Validation**: Catch SQL syntax errors during compilation when possible.
- **Modern C++ Design**: Leverages C++23 features like concepts, compile-time evaluation, and more.

Using SQLlib provides several advantages when learning SQL:

1. **Immediate Feedback**: Compiler errors help catch SQL syntax mistakes early.
2. **Discoverable API**: IDE auto-completion helps you discover SQL capabilities.
3. **Type Safety**: The compiler prevents type errors that might occur in raw SQL.
4. **SQL Injection Prevention**: Parameters are automatically bound safely.
5. **Learning Progressive**: You can focus on SQL concepts without worrying about string manipulation.

### Setting Up Your Environment

To get started with SQLlib, you'll need:

1. **A Modern C++ Compiler**:
   - GCC 11+ or
   - Clang 14+ or
   - MSVC 19.29+ (Visual Studio 2022+)

2. **Build Tools**:
   - CMake 3.15+
   - Conan 2.0+ (for dependencies)

3. **Required Dependencies**:
   - Boost (header-only)
   - fmt
   - Google Test (for testing)

#### Installation Steps

1. Clone the SQLlib repository:
```bash
git clone https://github.com/yourusername/sqllib.git
cd sqllib
```

2. Build the project using make:
```bash
# Build using make
make build

# Run tests to ensure everything is working
make test
```

Once you have your environment set up, you're ready to start learning SQL through SQLlib!

In the next section, we'll dive into database fundamentals, exploring core concepts that will form the foundation of your SQL knowledge.

## 2. Understanding Database Fundamentals

### What is a Database?

A database is an organized collection of structured data, typically stored electronically in a computer system. Databases are designed to efficiently store, retrieve, update, and manage data. They range from simple collections of a few records to massive systems holding billions of entries.

Modern applications rely on databases to:
- Store user information
- Track inventory and transactions
- Record application events and metrics
- Maintain content and media
- Support search functionality
- And much more

While many types of databases exist (NoSQL, graph, document, etc.), SQL is primarily used with **relational databases**, which we'll focus on in this guide.

### Relational Database Concepts

The relational database model, introduced by E.F. Codd in 1970, organizes data into tables (relations) with rows and columns. Key concepts include:

1. **Relations (Tables)**: Collections of related data organized in rows and columns.

2. **Tuples (Rows)**: Individual records within a table, each representing a unique entity or instance.

3. **Attributes (Columns)**: Properties or characteristics that define what data each row contains.

4. **Schema**: The structure that defines the organization of data in a database, including tables, columns, data types, and relationships.

5. **Relationships**: Connections between different tables, typically established through keys.

6. **Normalization**: The process of organizing data to reduce redundancy and improve data integrity.

7. **ACID Properties**: Guarantees that database transactions are processed reliably:
   - **Atomicity**: Transactions are all-or-nothing
   - **Consistency**: Transactions bring the database from one valid state to another
   - **Isolation**: Concurrent transactions don't interfere with each other
   - **Durability**: Completed transactions persist even in case of system failure

### Tables, Rows, and Columns

The table is the fundamental structure in a relational database. Let's examine these concepts more closely:

#### Tables

A table represents a single entity type or concept, such as:
- Users
- Products
- Orders
- Blog posts
- Comments

In SQLlib, tables are defined as C++ structs with column members:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    // Column definitions will go here
};
```

#### Rows

Rows (also called records or tuples) are individual instances of the entity represented by the table. For example, in a `users` table, each row represents a single user with their specific attributes.

When working with SQLlib, rows are typically handled as objects returned from query results.

#### Columns

Columns define the attributes or properties that each entity (row) in the table possesses. For example, a `users` table might have columns for:
- `id`
- `username`
- `email`
- `password`
- `created_at`

In SQLlib, columns are defined within table structs using the column template:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
};
```

### Primary Keys and Foreign Keys

#### Primary Keys

A primary key is a column (or combination of columns) that uniquely identifies each row in a table. Primary keys:
- Must contain a unique value for each row
- Cannot contain NULL values
- Should change rarely, if ever
- Should be simple and efficient

Common primary key strategies include:
- **Auto-incrementing integers**: Simple, efficient, doesn't expose business information
- **Natural keys**: Using existing unique data (e.g., ISBN for books)
- **Composite keys**: Combining multiple columns to form a unique identifier
- **UUIDs/GUIDs**: Universally unique identifiers, useful in distributed systems

In SQLlib, primary keys are defined using the `primary_key` template:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    
    // Define id as the primary key
    sqllib::schema::primary_key<&Users::id> pk;
};
```

#### Foreign Keys

Foreign keys establish relationships between tables by referencing the primary key of another table. They:
- Create referential integrity (ensuring related records exist)
- Define relationships between entities
- Enable joining tables in queries

For example, in a blog application:
- A `posts` table might have a `user_id` foreign key referencing the `id` primary key in the `users` table
- This establishes that each post is created by a specific user

In SQLlib, foreign keys are defined using the `foreign_key` template:

```cpp
struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    
    // Define id as the primary key
    sqllib::schema::primary_key<&Posts::id> pk;
    
    // Define user_id as a foreign key referencing Users::id
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};
```

### Data Types in SQL

SQL databases support various data types to store different kinds of information. Here are the most common categories:

#### Numeric Types
- **INTEGER**: Whole numbers without decimals (e.g., 1, 42, -10)
- **DECIMAL/NUMERIC**: Fixed-precision numbers (e.g., 10.5, 123.45)
- **FLOAT/REAL**: Floating-point numbers for scientific/engineering calculations
- **BOOLEAN**: True/false values (sometimes implemented as 0/1 integers)

#### String Types
- **CHAR(n)**: Fixed-length strings (padded with spaces)
- **VARCHAR(n)**: Variable-length strings up to n characters
- **TEXT**: Longer text data (articles, descriptions)

#### Date and Time Types
- **DATE**: Calendar date (e.g., 2023-07-15)
- **TIME**: Time of day (e.g., 14:30:00)
- **DATETIME/TIMESTAMP**: Combined date and time
- **INTERVAL**: Periods of time

#### Binary Types
- **BLOB**: Binary large objects (images, files, etc.)
- **BINARY/VARBINARY**: Binary data of fixed or variable length

#### Special Types
- **JSON**: Structured data in JSON format (in modern databases)
- **ARRAY**: Collections of values (in some databases)
- **UUID**: Universally unique identifiers
- **ENUM**: Restricted set of string values
- **XML**: Structured data in XML format (in some databases)

In SQLlib, C++ types are mapped to SQL types through the type system:

```cpp
// Common type mappings in SQLlib
sqllib::schema::column<"id", int> id;                      // Maps to INTEGER
sqllib::schema::column<"price", double> price;             // Maps to REAL or DOUBLE
sqllib::schema::column<"name", std::string> name;          // Maps to VARCHAR or TEXT
sqllib::schema::column<"is_active", bool> is_active;       // Maps to BOOLEAN
sqllib::schema::column<"created_at", std::chrono::system_clock::time_point> created_at; // Maps to TIMESTAMP
```

#### Nullable Types

In databases, columns can be declared as allowing NULL values (indicating missing or unknown data) or NOT NULL (requiring a value). In SQLlib, nullable columns are represented using `std::optional`:

```cpp
// A nullable column
sqllib::schema::column<"bio", std::optional<std::string>> bio;
```

Understanding these fundamental database concepts provides the foundation for working with SQL and SQLlib effectively. In the next section, we'll build on this knowledge to define our first database schema using SQLlib's type-safe approach.

## 3. Defining Database Schema with SQLlib

Database schema design is a crucial step in any database-driven application. In traditional SQL, you would define your schema using Data Definition Language (DDL) statements like `CREATE TABLE`. With SQLlib, we define schemas using C++ code, gaining type safety and compile-time validation.

### Creating Your First Table Schema

Let's start by creating a simple database schema for a blog application. We'll begin with a basic `Users` table:

```cpp
#include <sqllib/schema.hpp>
#include <string>

struct Users {
    // Table name (required)
    static constexpr auto table_name = "users";
    
    // Column definitions
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"password_hash", std::string> password_hash;
    sqllib::schema::column<"created_at", std::string> created_at;
    
    // Primary key
    sqllib::schema::primary_key<&Users::id> pk;
};
```

This C++ struct defines a SQL table with the following characteristics:
- Table name: `users`
- Columns: `id`, `username`, `email`, `password_hash`, `created_at`
- Primary key: `id`

In traditional SQL, this would be equivalent to:

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL,
    email TEXT NOT NULL,
    password_hash TEXT NOT NULL,
    created_at TEXT NOT NULL
);
```

### Defining Columns and Data Types

Let's explore column definitions in more detail:

```cpp
// Basic column syntax
sqllib::schema::column<"column_name", cpp_type> column_name;
```

The template parameters specify:
1. `"column_name"` - The SQL name of the column (as a string literal)
2. `cpp_type` - The C++ type for the column, which maps to an SQL type

SQLlib automatically maps C++ types to appropriate SQL types:

```cpp
// Integer types
sqllib::schema::column<"id", int> id;                     // INTEGER
sqllib::schema::column<"count", int64_t> count;           // BIGINT

// Floating point
sqllib::schema::column<"price", double> price;            // REAL or DOUBLE PRECISION
sqllib::schema::column<"ratio", float> ratio;             // REAL

// String types
sqllib::schema::column<"name", std::string> name;         // TEXT or VARCHAR
sqllib::schema::column<"code", std::string_view> code;    // TEXT or VARCHAR

// Boolean
sqllib::schema::column<"active", bool> active;            // BOOLEAN

// Date/Time (with proper includes)
sqllib::schema::column<"created", std::chrono::system_clock::time_point> created; // TIMESTAMP
```

### Working with Primary Keys

Primary keys uniquely identify each row in a table. SQLlib supports both single-column and composite primary keys:

#### Single-Column Primary Keys

Most tables use a single column (often an auto-incrementing integer) as a primary key:

```cpp
struct Products {
    static constexpr auto table_name = "products";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"price", double> price;
    
    // Single-column primary key
    sqllib::schema::primary_key<&Products::id> pk;
};
```

#### Composite Primary Keys

Sometimes, a combination of columns uniquely identifies a row:

```cpp
struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    sqllib::schema::column<"order_id", int> order_id;
    sqllib::schema::column<"product_id", int> product_id;
    sqllib::schema::column<"quantity", int> quantity;
    sqllib::schema::column<"price", double> price;
    
    // Composite primary key (order_id + product_id)
    sqllib::schema::composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
};
```

This would be equivalent to:

```sql
CREATE TABLE order_items (
    order_id INTEGER,
    product_id INTEGER,
    quantity INTEGER NOT NULL,
    price REAL NOT NULL,
    PRIMARY KEY (order_id, product_id)
);
```

### Creating Foreign Key Relationships

Foreign keys establish relationships between tables. They ensure referential integrity by verifying that referenced rows exist:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    
    sqllib::schema::primary_key<&Users::id> pk;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    
    // Primary key
    sqllib::schema::primary_key<&Posts::id> pk;
    
    // Foreign key reference to Users table
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

struct Comments {
    static constexpr auto table_name = "comments";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"content", std::string> content;
    
    // Primary key
    sqllib::schema::primary_key<&Comments::id> pk;
    
    // Multiple foreign keys
    sqllib::schema::foreign_key<&Comments::post_id, &Posts::id> post_fk;
    sqllib::schema::foreign_key<&Comments::user_id, &Users::id> user_fk;
};
```

In this example:
- `Posts` has a foreign key to `Users` (each post belongs to a user)
- `Comments` has foreign keys to both `Posts` and `Users` (each comment belongs to a post and a user)

### Adding Constraints and Indexes

SQLlib supports various constraints and indexes to ensure data integrity and optimize query performance.

#### Unique Constraints

Unique constraints ensure that values in specified columns are unique across all rows:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    
    // Primary key
    sqllib::schema::primary_key<&Users::id> pk;
    
    // Unique constraints
    sqllib::schema::unique_constraint<&Users::username> unique_username;
    sqllib::schema::unique_constraint<&Users::email> unique_email;
};
```

This ensures that no two users can have the same username or email.

#### Check Constraints

Check constraints enforce specific conditions on column values:

```cpp
struct Products {
    static constexpr auto table_name = "products";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"price", double> price;
    sqllib::schema::column<"stock", int> stock;
    
    // Primary key
    sqllib::schema::primary_key<&Products::id> pk;
    
    // Check constraints
    sqllib::schema::check_constraint<&Products::price, "price > 0"> positive_price;
    sqllib::schema::check_constraint<&Products::stock, "stock >= 0"> non_negative_stock;
};
```

The check constraints ensure that:
- `price` is always greater than 0
- `stock` is never negative

#### Default Values

Default values are applied when no value is specified during insertion:

```cpp
struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"views", int, sqllib::schema::DefaultValue<0>> views;
    sqllib::schema::column<"published", bool, sqllib::schema::DefaultValue<false>> published;
    sqllib::schema::column<"created_at", std::string, sqllib::schema::DefaultValue<"CURRENT_TIMESTAMP">> created_at;
    
    // Primary key
    sqllib::schema::primary_key<&Posts::id> pk;
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};
```

In this example:
- `views` defaults to 0
- `published` defaults to false
- `created_at` defaults to the current timestamp

#### Indexes

Indexes improve query performance for frequently used columns:

```cpp
struct Orders {
    static constexpr auto table_name = "orders";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"order_date", std::string> order_date;
    sqllib::schema::column<"status", std::string> status;
    sqllib::schema::column<"total", double> total;
    
    // Primary key
    sqllib::schema::primary_key<&Orders::id> pk;
    
    // Foreign key
    sqllib::schema::foreign_key<&Orders::user_id, &Users::id> user_fk;
    
    // Indexes to improve query performance
    sqllib::schema::index<&Orders::user_id> user_id_idx;
    sqllib::schema::index<&Orders::order_date> order_date_idx;
    sqllib::schema::index<&Orders::status> status_idx;
};
```

There are different types of indexes:

```cpp
// Regular index
sqllib::schema::index<&Table::column> idx1;

// Unique index
sqllib::schema::index<&Table::column> idx2{sqllib::schema::index_type::unique};

// Composite index (multiple columns)
sqllib::schema::composite_index<&Table::col1, &Table::col2> idx3;
```

### Nullable Fields with std::optional

In databases, columns can be NULL to represent missing or unknown values. In SQLlib, nullable columns are defined using `std::optional`:

```cpp
#include <optional>

struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    
    // Nullable columns with std::optional
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
    sqllib::schema::column<"phone", std::optional<std::string>> phone;
    sqllib::schema::column<"last_login", std::optional<std::string>> last_login;
    
    sqllib::schema::primary_key<&Users::id> pk;
};
```

In this example:
- `bio`, `phone`, and `last_login` can be NULL in the database
- The other columns are NOT NULL

In traditional SQL, this would be:

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL,
    email TEXT NOT NULL,
    bio TEXT NULL,
    phone TEXT NULL,
    last_login TEXT NULL
);
```

### Putting It All Together: A Complete Blog Schema

Let's create a complete schema for a simple blog application:

```cpp
#include <sqllib/schema.hpp>
#include <string>
#include <optional>

// Users table
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"password_hash", std::string> password_hash;
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
    sqllib::schema::column<"created_at", std::string, sqllib::schema::DefaultValue<"CURRENT_TIMESTAMP">> created_at;
    
    sqllib::schema::primary_key<&Users::id> pk;
    sqllib::schema::unique_constraint<&Users::username> unique_username;
    sqllib::schema::unique_constraint<&Users::email> unique_email;
};

// Categories table
struct Categories {
    static constexpr auto table_name = "categories";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"description", std::optional<std::string>> description;
    
    sqllib::schema::primary_key<&Categories::id> pk;
    sqllib::schema::unique_constraint<&Categories::name> unique_name;
};

// Posts table
struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"category_id", std::optional<int>> category_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"published", bool, sqllib::schema::DefaultValue<false>> published;
    sqllib::schema::column<"views", int, sqllib::schema::DefaultValue<0>> views;
    sqllib::schema::column<"created_at", std::string, sqllib::schema::DefaultValue<"CURRENT_TIMESTAMP">> created_at;
    sqllib::schema::column<"updated_at", std::optional<std::string>> updated_at;
    
    sqllib::schema::primary_key<&Posts::id> pk;
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
    sqllib::schema::foreign_key<&Posts::category_id, &Categories::id> category_fk;
    
    sqllib::schema::index<&Posts::user_id> user_id_idx;
    sqllib::schema::index<&Posts::created_at> created_at_idx;
};

// Comments table
struct Comments {
    static constexpr auto table_name = "comments";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"created_at", std::string, sqllib::schema::DefaultValue<"CURRENT_TIMESTAMP">> created_at;
    
    sqllib::schema::primary_key<&Comments::id> pk;
    sqllib::schema::foreign_key<&Comments::post_id, &Posts::id> post_fk;
    sqllib::schema::foreign_key<&Comments::user_id, &Users::id> user_fk;
    
    sqllib::schema::index<&Comments::post_id> post_id_idx;
    sqllib::schema::index<&Comments::user_id> user_id_idx;
};

// Tags table
struct Tags {
    static constexpr auto table_name = "tags";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    
    sqllib::schema::primary_key<&Tags::id> pk;
    sqllib::schema::unique_constraint<&Tags::name> unique_name;
};

// Post_Tags junction table (for many-to-many relationship)
struct PostTags {
    static constexpr auto table_name = "post_tags";
    
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"tag_id", int> tag_id;
    
    // Composite primary key
    sqllib::schema::composite_primary_key<&PostTags::post_id, &PostTags::tag_id> pk;
    
    // Foreign keys
    sqllib::schema::foreign_key<&PostTags::post_id, &Posts::id> post_fk;
    sqllib::schema::foreign_key<&PostTags::tag_id, &Tags::id> tag_fk;
};
```

This comprehensive schema demonstrates:
- Multiple interconnected tables
- Primary and foreign keys
- Unique constraints
- Indexes for performance optimization
- Default values
- Nullable columns
- A junction table for a many-to-many relationship

With SQLlib, we've defined a complete, type-safe database schema that the compiler can validate, helping catch errors early in the development process.

In the next section, we'll learn how to query this database using SQLlib's fluent query-building interface.

## 4. Basic SQL Queries with SQLlib

Now that we've defined our database schema, it's time to learn how to retrieve data from the database using SQL queries. The SELECT statement is the foundation of SQL queries and is used to fetch data from one or more tables.

### Your First SELECT Query

The most basic SELECT query retrieves all columns from a table. In traditional SQL, this looks like:

```sql
SELECT * FROM users;
```

With SQLlib, we build queries using a fluent interface that chains method calls together. Here's how to create the same query:

```cpp
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>

// First, create an instance of our table
Users u;

// Then, build a query using the select function
auto query = sqllib::query::select_all()
    .from(u);

// The generated SQL would be: SELECT * FROM users
```

If you want to select specific columns instead of all columns (which is generally a best practice), you can specify them:

```cpp
// In traditional SQL:
// SELECT id, username, email FROM users;

auto query = sqllib::query::select(u.id, u.username, u.email)
    .from(u);
```

To execute this query and get results, you would use a database connection:

```cpp
// Assuming you have a connection object
auto results = connection.execute(query);

// Process results
for (const auto& row : results) {
    int id = row.get<0>();  // Get the first column (id)
    std::string username = row.get<1>();  // Get the second column (username)
    std::string email = row.get<2>();  // Get the third column (email)
    
    // Do something with the data...
    std::cout << "User: " << username << " (" << email << ")" << std::endl;
}
```

### Understanding Query Building

SQLlib's query building system uses method chaining to construct SQL queries in a readable, fluent style. The general pattern is:

```cpp
auto query = sqllib::query::select(...)  // Start with SELECT
    .from(...)                         // Add FROM clause
    .where(...)                        // Optionally add WHERE clause
    .order_by(...)                     // Optionally add ORDER BY clause
    .limit(...);                       // Optionally add LIMIT clause
```

Each method returns a new query object with the added clause, allowing you to chain them together. This approach makes queries easy to build incrementally and provides natural syntax that mirrors SQL.

### Filtering Data with WHERE Clauses

The WHERE clause filters rows based on specific conditions. Let's see how to add conditions to our queries:

#### Simple Equality Conditions

```cpp
// In traditional SQL:
// SELECT id, username FROM users WHERE id = 1;

auto query = sqllib::query::select(u.id, u.username)
    .from(u)
    .where(sqllib::query::to_expr(u.id) == sqllib::query::val(1));
```

The `to_expr()` function converts a column to an expression that can be used in conditions. The `val()` function creates a value expression with the specified value.

#### Comparison Operators

SQLlib supports all standard SQL comparison operators:

```cpp
// Greater than
// SELECT * FROM users WHERE age > 18;
auto query1 = sqllib::query::select_all()
    .from(u)
    .where(sqllib::query::to_expr(u.age) > sqllib::query::val(18));

// Less than or equal
// SELECT * FROM products WHERE price <= 100.0;
auto query2 = sqllib::query::select_all()
    .from(p)
    .where(sqllib::query::to_expr(p.price) <= sqllib::query::val(100.0));

// Not equal
// SELECT * FROM orders WHERE status != 'Completed';
auto query3 = sqllib::query::select_all()
    .from(o)
    .where(sqllib::query::to_expr(o.status) != sqllib::query::val("Completed"));
```

#### Multiple Conditions with AND/OR

You can combine conditions using logical operators:

```cpp
// Using AND
// SELECT * FROM users WHERE age >= 18 AND age < 65;
auto query1 = sqllib::query::select_all()
    .from(u)
    .where(
        (sqllib::query::to_expr(u.age) >= sqllib::query::val(18)) &&
        (sqllib::query::to_expr(u.age) < sqllib::query::val(65))
    );

// Using OR
// SELECT * FROM products WHERE category = 'Electronics' OR price > 1000;
auto query2 = sqllib::query::select_all()
    .from(p)
    .where(
        (sqllib::query::to_expr(p.category) == sqllib::query::val("Electronics")) ||
        (sqllib::query::to_expr(p.price) > sqllib::query::val(1000.0))
    );

// Combining AND and OR with parentheses
// SELECT * FROM orders 
// WHERE (status = 'Pending' OR status = 'Processing') 
// AND total > 100;
auto query3 = sqllib::query::select_all()
    .from(o)
    .where(
        (
            (sqllib::query::to_expr(o.status) == sqllib::query::val("Pending")) ||
            (sqllib::query::to_expr(o.status) == sqllib::query::val("Processing"))
        ) &&
        (sqllib::query::to_expr(o.total) > sqllib::query::val(100.0))
    );
```

#### Pattern Matching with LIKE

The LIKE operator searches for patterns in text columns:

```cpp
// Search for usernames starting with 'j'
// SELECT * FROM users WHERE username LIKE 'j%';
auto query1 = sqllib::query::select_all()
    .from(u)
    .where(sqllib::query::like(
        sqllib::query::to_expr(u.username), 
        "j%"
    ));

// Search for emails containing 'example.com'
// SELECT * FROM users WHERE email LIKE '%example.com';
auto query2 = sqllib::query::select_all()
    .from(u)
    .where(sqllib::query::like(
        sqllib::query::to_expr(u.email), 
        "%example.com"
    ));
```

In the pattern:
- `%` matches any sequence of characters (including none)
- `_` matches exactly one character

#### NULL Handling

SQL uses NULL to represent missing or unknown values. To check for NULL or NOT NULL:

```cpp
// Find users with no bio
// SELECT * FROM users WHERE bio IS NULL;
auto query1 = sqllib::query::select_all()
    .from(u)
    .where(sqllib::query::is_null(sqllib::query::to_expr(u.bio)));

// Find users who have provided a bio
// SELECT * FROM users WHERE bio IS NOT NULL;
auto query2 = sqllib::query::select_all()
    .from(u)
    .where(sqllib::query::is_not_null(sqllib::query::to_expr(u.bio)));
```

#### IN Operator

The IN operator checks if a value matches any value in a list:

```cpp
// Find posts in selected categories
// SELECT * FROM posts WHERE category_id IN (1, 2, 3);
auto query = sqllib::query::select_all()
    .from(p)
    .where(sqllib::query::in(
        sqllib::query::to_expr(p.category_id),
        std::vector<int>{1, 2, 3}
    ));
```

### Sorting Data with ORDER BY

The ORDER BY clause sorts the results based on one or more columns:

```cpp
// Sort users by username (alphabetically)
// SELECT * FROM users ORDER BY username;
auto query1 = sqllib::query::select_all()
    .from(u)
    .order_by(sqllib::query::to_expr(u.username));

// Sort in descending order (newest users first)
// SELECT * FROM users ORDER BY created_at DESC;
auto query2 = sqllib::query::select_all()
    .from(u)
    .order_by(sqllib::query::to_expr(u.created_at), sqllib::query::order::desc);

// Sort by multiple columns (first by status, then by total)
// SELECT * FROM orders ORDER BY status ASC, total DESC;
auto query3 = sqllib::query::select_all()
    .from(o)
    .order_by(sqllib::query::to_expr(o.status), sqllib::query::order::asc)
    .order_by(sqllib::query::to_expr(o.total), sqllib::query::order::desc);
```

### Limiting Results

The LIMIT clause restricts the number of rows returned by a query, which is useful for pagination or getting just the top/bottom N rows:

```cpp
// Get the 10 most recent posts
// SELECT * FROM posts ORDER BY created_at DESC LIMIT 10;
auto query1 = sqllib::query::select_all()
    .from(p)
    .order_by(sqllib::query::to_expr(p.created_at), sqllib::query::order::desc)
    .limit(10);

// Pagination example: Get the second page of 20 results
// SELECT * FROM products LIMIT 20 OFFSET 20;
auto query2 = sqllib::query::select_all()
    .from(p)
    .limit(20)
    .offset(20);  // Skip the first 20 rows (page 1)
```

### Putting It All Together: Blog Application Queries

Let's create some practical queries for our blog application:

```cpp
// Include necessary headers
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>

// Create table instances
Users u;
Posts p;
Categories c;
Comments cm;
Tags t;
PostTags pt;

// 1. Get all published posts with their authors
auto published_posts_query = sqllib::query::select(
        p.id, p.title, p.content, p.created_at,
        u.id, u.username
    )
    .from(p)
    .join(u, sqllib::query::on(sqllib::query::to_expr(p.user_id) == sqllib::query::to_expr(u.id)))
    .where(sqllib::query::to_expr(p.published) == sqllib::query::val(true))
    .order_by(sqllib::query::to_expr(p.created_at), sqllib::query::order::desc);

// 2. Get the 5 most recent posts in a specific category
auto category_posts_query = sqllib::query::select(
        p.id, p.title, p.created_at
    )
    .from(p)
    .where(
        (sqllib::query::to_expr(p.category_id) == sqllib::query::val(1)) &&
        (sqllib::query::to_expr(p.published) == sqllib::query::val(true))
    )
    .order_by(sqllib::query::to_expr(p.created_at), sqllib::query::order::desc)
    .limit(5);

// 3. Search for posts with titles containing a keyword
auto search_query = sqllib::query::select(
        p.id, p.title
    )
    .from(p)
    .where(
        (sqllib::query::like(sqllib::query::to_expr(p.title), "%keyword%")) &&
        (sqllib::query::to_expr(p.published) == sqllib::query::val(true))
    )
    .order_by(sqllib::query::to_expr(p.created_at), sqllib::query::order::desc);

// 4. Get user profile with post count
auto user_profile_query = sqllib::query::select(
        u.id, u.username, u.email, u.bio,
        sqllib::query::as(sqllib::query::count(sqllib::query::to_expr(p.id)), "post_count")
    )
    .from(u)
    .left_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
    .where(sqllib::query::to_expr(u.id) == sqllib::query::val(1))
    .group_by(sqllib::query::to_expr(u.id));

// 5. Get all posts with their tags
auto posts_with_tags_query = sqllib::query::select(
        p.id, p.title, t.id, t.name
    )
    .from(p)
    .join(pt, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(pt.post_id)))
    .join(t, sqllib::query::on(sqllib::query::to_expr(pt.tag_id) == sqllib::query::to_expr(t.id)))
    .where(sqllib::query::to_expr(p.published) == sqllib::query::val(true))
    .order_by(sqllib::query::to_expr(p.created_at), sqllib::query::order::desc);
```

These examples demonstrate how to use SQLlib to create practical database queries for a real-world application. We've covered basic SELECT statements, filtering with WHERE, sorting with ORDER BY, and limiting results with LIMIT.

In the next section, we'll explore how to work with multiple tables using JOINs, which will allow us to combine data from related tables.

## 5. Working with Multiple Tables

Real-world databases rarely consist of a single isolated table. Instead, data is distributed across multiple tables that are related to each other. In this section, we'll learn how to combine data from related tables to answer more complex questions.

### Understanding Table Relationships

Before diving into JOINs, let's understand the types of relationships that can exist between tables:

1. **One-to-One (1:1)**: Each record in the first table corresponds to exactly one record in the second table.
   - Example: A `users` table and a `user_profiles` table where each user has exactly one profile.

2. **One-to-Many (1:N)**: Each record in the first table can correspond to multiple records in the second table, but each record in the second table corresponds to only one record in the first table.
   - Example: A `users` table and a `posts` table where one user can create many posts, but each post is created by exactly one user.

3. **Many-to-Many (N:M)**: Each record in the first table can correspond to multiple records in the second table, and vice versa.
   - Example: A `posts` table and a `tags` table where each post can have multiple tags, and each tag can be applied to multiple posts.
   - Many-to-many relationships typically require a junction table (like our `post_tags` table) to implement.

These relationships are established through foreign keys, which we defined in our schema in Section 3.

### Introduction to JOINs

A JOIN combines rows from two or more tables based on a related column between them. JOINs are essential for working with normalized data spread across multiple tables.

The basic syntax for a JOIN in traditional SQL is:

```sql
SELECT columns
FROM table1
JOIN table2 ON table1.column = table2.column;
```

There are several types of JOINs:

1. **INNER JOIN**: Returns only rows that have matching values in both tables.
2. **LEFT JOIN** (or LEFT OUTER JOIN): Returns all rows from the left table and matching rows from the right table.
3. **RIGHT JOIN** (or RIGHT OUTER JOIN): Returns all rows from the right table and matching rows from the left table.
4. **FULL JOIN** (or FULL OUTER JOIN): Returns all rows when there's a match in either the left or right table.
5. **CROSS JOIN**: Returns the Cartesian product (all possible combinations) of rows from both tables.

The most commonly used types are INNER JOIN and LEFT JOIN, which we'll focus on in this section.

### INNER JOINs with SQLlib

An INNER JOIN returns only the rows that have matching values in both tables. Let's start with a basic example:

```cpp
// Get posts with their authors
// In traditional SQL:
// SELECT p.id, p.title, u.username
// FROM posts p
// INNER JOIN users u ON p.user_id = u.id;

Users u;
Posts p;

auto query = sqllib::query::select(p.id, p.title, u.username)
    .from(p)
    .join(u, sqllib::query::on(sqllib::query::to_expr(p.user_id) == sqllib::query::to_expr(u.id)));
```

In SQLlib, `.join()` performs an INNER JOIN by default. The `on()` function specifies the join condition, typically comparing a foreign key in one table to a primary key in another.

Let's look at more complex examples with multiple joins:

```cpp
// Get comments with post titles and usernames
// SELECT c.content, p.title, u.username
// FROM comments c
// INNER JOIN posts p ON c.post_id = p.id
// INNER JOIN users u ON c.user_id = u.id;

Users u;
Posts p;
Comments c;

auto query = sqllib::query::select(c.content, p.title, u.username)
    .from(c)
    .join(p, sqllib::query::on(sqllib::query::to_expr(c.post_id) == sqllib::query::to_expr(p.id)))
    .join(u, sqllib::query::on(sqllib::query::to_expr(c.user_id) == sqllib::query::to_expr(u.id)));
```

You can also add WHERE conditions after JOINs:

```cpp
// Get posts in a specific category with author names
// SELECT p.title, u.username, c.name
// FROM posts p
// INNER JOIN users u ON p.user_id = u.id
// INNER JOIN categories c ON p.category_id = c.id
// WHERE c.id = 1;

Users u;
Posts p;
Categories c;

auto query = sqllib::query::select(p.title, u.username, c.name)
    .from(p)
    .join(u, sqllib::query::on(sqllib::query::to_expr(p.user_id) == sqllib::query::to_expr(u.id)))
    .join(c, sqllib::query::on(sqllib::query::to_expr(p.category_id) == sqllib::query::to_expr(c.id)))
    .where(sqllib::query::to_expr(c.id) == sqllib::query::val(1));
```

### LEFT and RIGHT JOINs

A LEFT JOIN returns all rows from the left table and matching rows from the right table. If there's no match, NULL values are returned for the right table's columns.

```cpp
// Get all users and their posts (if any)
// SELECT u.id, u.username, p.id, p.title
// FROM users u
// LEFT JOIN posts p ON u.id = p.user_id;

Users u;
Posts p;

auto query = sqllib::query::select(u.id, u.username, p.id, p.title)
    .from(u)
    .left_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));
```

This query will include all users, even those who haven't created any posts.

RIGHT JOINs work similarly but return all rows from the right table:

```cpp
// Get all categories and their posts (if any)
// SELECT c.name, p.title
// FROM posts p
// RIGHT JOIN categories c ON p.category_id = c.id;

Posts p;
Categories c;

auto query = sqllib::query::select(c.name, p.title)
    .from(p)
    .right_join(c, sqllib::query::on(sqllib::query::to_expr(p.category_id) == sqllib::query::to_expr(c.id)));
```

This query will include all categories, even those that don't have any posts.

> **Note**: Some database systems (like SQLite) don't support RIGHT JOIN directly. In those cases, you can achieve the same result by swapping the tables and using LEFT JOIN.

### Many-to-Many Relationships with JOINs

For many-to-many relationships, we need to join through a junction table:

```cpp
// Get all posts with their tags
// SELECT p.title, t.name
// FROM posts p
// INNER JOIN post_tags pt ON p.id = pt.post_id
// INNER JOIN tags t ON pt.tag_id = t.id;

Posts p;
PostTags pt;
Tags t;

auto query = sqllib::query::select(p.title, t.name)
    .from(p)
    .join(pt, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(pt.post_id)))
    .join(t, sqllib::query::on(sqllib::query::to_expr(pt.tag_id) == sqllib::query::to_expr(t.id)));
```

This query connects posts to tags through the junction table `post_tags`.

### Self-Joins

Sometimes you need to join a table to itself, for example, to represent hierarchical data:

```cpp
// Assuming we have an employees table with a manager_id FK referring to another employee
struct Employees {
    static constexpr auto table_name = "employees";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"manager_id", std::optional<int>> manager_id;
    
    sqllib::schema::primary_key<&Employees::id> pk;
    sqllib::schema::foreign_key<&Employees::manager_id, &Employees::id> manager_fk;
};

// To get employees with their managers' names:
// SELECT e.name as employee, m.name as manager
// FROM employees e
// LEFT JOIN employees m ON e.manager_id = m.id;

// We need two instances of the same table
Employees e; // employees
Employees m; // managers

auto query = sqllib::query::select(
        sqllib::query::as(sqllib::query::to_expr(e.name), "employee"),
        sqllib::query::as(sqllib::query::to_expr(m.name), "manager")
    )
    .from(e)
    .left_join(m, sqllib::query::on(sqllib::query::to_expr(e.manager_id) == sqllib::query::to_expr(m.id)));
```

In this example, we're joining the employees table to itself to find each employee's manager.

### USING Clause for Simpler Joins

When the columns used for joining have the same name in both tables, you can use the USING clause for a more concise syntax:

```sql
-- Traditional SQL with USING
SELECT p.title, c.name
FROM posts p
JOIN categories c USING (category_id);
```

In SQLlib, we can achieve similar simplicity with helper functions:

```cpp
// Similar to USING clause when column names match
Posts p;
Categories c;

auto query = sqllib::query::select(p.title, c.name)
    .from(p)
    .join(c, sqllib::query::on_using(p.category_id, c.id));
```

### Using Foreign Keys in Queries

Since we've defined foreign key relationships in our schema, SQLlib can potentially use this information to simplify join conditions. Here's how we might leverage our defined relationships:

```cpp
// Using predefined foreign key relationships
Posts p;
Users u;

// In a hypothetical enhancement of SQLlib:
auto query = sqllib::query::select(p.title, u.username)
    .from(p)
    .join_using_fk<&Posts::user_fk>(u);
```

This would automatically use the foreign key relationship we defined between `Posts` and `Users` to create the join.

> Note: This specific syntax is conceptual and might not exactly match SQLlib's implementation. The idea is that the schema definitions could help simplify query writing.

### Combining Multiple Join Types

You can combine different types of joins in a single query:

```cpp
// Get all users, their posts (if any), and categories (if specified)
// SELECT u.username, p.title, c.name
// FROM users u
// LEFT JOIN posts p ON u.id = p.user_id
// LEFT JOIN categories c ON p.category_id = c.id;

Users u;
Posts p;
Categories c;

auto query = sqllib::query::select(u.username, p.title, c.name)
    .from(u)
    .left_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
    .left_join(c, sqllib::query::on(sqllib::query::to_expr(p.category_id) == sqllib::query::to_expr(c.id)));
```

This query will include all users, whether they have posts or not, and will include category information for posts that have a category.

### Practical Examples: Blog Application Queries

Let's create some comprehensive queries for our blog application that utilize JOINs:

```cpp
// Include necessary headers
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>

// Create table instances
Users u;
Posts p;
Categories c;
Comments cm;
Tags t;
PostTags pt;

// 1. Get posts with author and category information
auto blog_posts_query = sqllib::query::select(
        p.id, p.title, p.content, p.created_at,
        u.username, u.email,
        c.name.as("category_name")
    )
    .from(p)
    .join(u, sqllib::query::on(sqllib::query::to_expr(p.user_id) == sqllib::query::to_expr(u.id)))
    .left_join(c, sqllib::query::on(sqllib::query::to_expr(p.category_id) == sqllib::query::to_expr(c.id)))
    .where(sqllib::query::to_expr(p.published) == sqllib::query::val(true))
    .order_by(sqllib::query::to_expr(p.created_at), sqllib::query::order::desc);

// 2. Get post details including comments count and tags
auto post_details_query = sqllib::query::select(
        p.id, p.title, p.content, u.username,
        sqllib::query::as(sqllib::query::count(sqllib::query::distinct(cm.id)), "comment_count")
    )
    .from(p)
    .join(u, sqllib::query::on(sqllib::query::to_expr(p.user_id) == sqllib::query::to_expr(u.id)))
    .left_join(cm, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(cm.post_id)))
    .where(sqllib::query::to_expr(p.id) == sqllib::query::val(1))
    .group_by(sqllib::query::to_expr(p.id), sqllib::query::to_expr(p.title), 
             sqllib::query::to_expr(p.content), sqllib::query::to_expr(u.username));

// 3. Get users with their post counts and comment counts
auto user_activity_query = sqllib::query::select(
        u.id, u.username,
        sqllib::query::as(sqllib::query::count(sqllib::query::distinct(p.id)), "post_count"),
        sqllib::query::as(sqllib::query::count(sqllib::query::distinct(cm.id)), "comment_count")
    )
    .from(u)
    .left_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
    .left_join(cm, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(cm.user_id)))
    .group_by(sqllib::query::to_expr(u.id), sqllib::query::to_expr(u.username))
    .order_by(sqllib::query::as(sqllib::query::count(sqllib::query::distinct(p.id)), "post_count"), 
             sqllib::query::order::desc);

// 4. Find all tags used in posts by a specific user
auto user_tags_query = sqllib::query::select(
        sqllib::query::distinct(t.name)
    )
    .from(t)
    .join(pt, sqllib::query::on(sqllib::query::to_expr(t.id) == sqllib::query::to_expr(pt.tag_id)))
    .join(p, sqllib::query::on(sqllib::query::to_expr(pt.post_id) == sqllib::query::to_expr(p.id)))
    .where(sqllib::query::to_expr(p.user_id) == sqllib::query::val(1))
    .order_by(sqllib::query::to_expr(t.name));

// 5. Find posts that have specific tags (posts with both "technology" and "programming" tags)
auto multi_tag_posts_query = sqllib::query::select(
        sqllib::query::distinct(p.id), p.title
    )
    .from(p)
    .join(pt, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(pt.post_id)))
    .join(t, sqllib::query::on(sqllib::query::to_expr(pt.tag_id) == sqllib::query::to_expr(t.id)))
    .where(sqllib::query::to_expr(t.name) == sqllib::query::val("technology"))
    .intersect(
        sqllib::query::select(
            sqllib::query::distinct(p.id), p.title
        )
        .from(p)
        .join(pt, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(pt.post_id)))
        .join(t, sqllib::query::on(sqllib::query::to_expr(pt.tag_id) == sqllib::query::to_expr(t.id)))
        .where(sqllib::query::to_expr(t.name) == sqllib::query::val("programming"))
    );
```

These examples demonstrate the power of JOINs in SQLlib for working with related tables. We've combined data from multiple tables to create complex queries that would be difficult or impossible with single-table queries.

In the next section, we'll learn about data manipulation operations: inserting, updating, and deleting data using SQLlib.
