# Schema Definition in relx

relx provides a type-safe way to define database schemas as C++ structures. This document explains how to define tables, columns, and relationships.

## Table of Contents
- [Basic Table Definition](#basic-table-definition)
- [Column Types](#column-types)
- [Primary Keys](#primary-keys)
- [Foreign Keys](#foreign-keys)
- [Unique Constraints](#unique-constraints)
- [Nullable Columns](#nullable-columns)
- [Indexes](#indexes)
- [Composite Keys](#composite-keys)

## Basic Table Definition

A table in relx is defined as a C++ struct with table metadata and column definitions:

```cpp
#include <relx/schema.hpp>
#include <string>
#include <optional>

struct Users {
    // Table name (required)
    static constexpr auto table_name = "users";
    
    // Column definitions
    relx::schema::column<"id", int> id;
    relx::schema::column<"username", std::string> username;
    relx::schema::column<"email", std::string> email;
    relx::schema::column<"created_at", std::string> created_at;
    
    // Primary key constraint
    relx::schema::primary_key<&Users::id> pk;
    
    // Unique constraint
    relx::schema::unique_constraint<&Users::email> unique_email;
};
```

Key points:
- A table is represented as a struct with a static `table_name` field
- Each column is defined using the `relx::schema::column<Name, Type>` template
- The struct should also contain any constraints that apply to the table

## Column Types

relx automatically maps C++ types to SQL types:

| C++ Type | SQL Type |
|----------|----------|
| `int` | `INTEGER` |
| `double` | `REAL` |
| `std::string` | `TEXT` |
| `bool` | `INTEGER` (0 or 1) |
| `std::optional<T>` | SQL type of T, but allows NULL |

Example:

```cpp
struct Products {
    static constexpr auto table_name = "products";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"price", double> price;
    relx::schema::column<"is_active", bool> is_active;
    relx::schema::column<"description", std::optional<std::string>> description;
    
    relx::schema::primary_key<&Products::id> pk;
};
```

In this example:
- `id` maps to `INTEGER NOT NULL`
- `name` maps to `TEXT NOT NULL`
- `price` maps to `REAL NOT NULL`
- `is_active` maps to `INTEGER NOT NULL` (0 for false, 1 for true)
- `description` maps to `TEXT` (nullable)

## Primary Keys

Primary keys are defined using the `primary_key` template:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"username", std::string> username;
    
    // Primary key constraint
    relx::schema::primary_key<&Users::id> pk;
};
```

The template parameter is a pointer to the member variable that serves as the primary key.

## Foreign Keys

Foreign keys define relationships between tables:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    // ...
    
    relx::schema::primary_key<&Users::id> pk;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"user_id", int> user_id;
    // ...
    
    relx::schema::primary_key<&Posts::id> pk;
    
    // Foreign key from Posts.user_id to Users.id
    relx::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};
```

The `foreign_key` template takes two parameters:
1. Pointer to the column in the current table
2. Pointer to the referenced column in another table

## Unique Constraints

Unique constraints are defined similarly to primary keys:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"email", std::string> email;
    // ...
    
    relx::schema::primary_key<&Users::id> pk;
    
    // Unique constraint on email
    relx::schema::unique_constraint<&Users::email> unique_email;
};
```

The template parameter is a pointer to the member variable that must be unique.

## Nullable Columns

To define nullable columns, use `std::optional<T>`:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"bio", std::optional<std::string>> bio;
    relx::schema::column<"last_login", std::optional<std::string>> last_login;
    // ...
    
    relx::schema::primary_key<&Users::id> pk;
};
```

In this example, `bio` and `last_login` are nullable columns.

## Indexes

To define indexes for faster queries:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"email", std::string> email;
    // ...
    
    relx::schema::primary_key<&Users::id> pk;
    
    // Regular index on name (for fast searching)
    relx::schema::index<&Users::name> name_idx;
    
    // Unique index on email (for fast searching & enforcing uniqueness)
    relx::schema::unique_index<&Users::email> email_idx;
};
```

## Composite Keys

relx also supports composite (multi-column) keys:

```cpp
struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    relx::schema::column<"order_id", int> order_id;
    relx::schema::column<"product_id", int> product_id;
    relx::schema::column<"quantity", int> quantity;
    relx::schema::column<"price", double> price;
    
    // Composite primary key
    relx::schema::composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
};
```

Similarly, you can create composite foreign keys and composite unique constraints:

```cpp
struct Orders {
    static constexpr auto table_name = "orders";
    
    relx::schema::column<"id", int> id;
    // ...
    
    relx::schema::primary_key<&Orders::id> pk;
};

struct Products {
    static constexpr auto table_name = "products";
    
    relx::schema::column<"id", int> id;
    // ...
    
    relx::schema::primary_key<&Products::id> pk;
};

struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    relx::schema::column<"order_id", int> order_id;
    relx::schema::column<"product_id", int> product_id;
    // ...
    
    // Composite primary key
    relx::schema::composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
    
    // Foreign keys to parent tables
    relx::schema::foreign_key<&OrderItems::order_id, &Orders::id> order_fk;
    relx::schema::foreign_key<&OrderItems::product_id, &Products::id> product_fk;
};
```

## Automatic SQL Generation

relx can automatically generate SQL DDL (Data Definition Language) statements for creating tables:

```cpp
Users users;

// Generate CREATE TABLE statement
std::string create_table_sql = relx::schema::create_table(users);

// Output:
// CREATE TABLE users (
//   id INTEGER NOT NULL,
//   username TEXT NOT NULL,
//   email TEXT NOT NULL,
//   created_at TEXT NOT NULL,
//   PRIMARY KEY (id),
//   UNIQUE (email)
// )
```

You can use this to automatically create tables in your database:

```cpp
relx::SQLiteConnection conn("database.db");
conn.connect();

// Create tables
conn.execute_raw(relx::schema::create_table(users));
conn.execute_raw(relx::schema::create_table(posts));
``` 