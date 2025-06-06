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
    relx::column<Users, "id", int> id;
    relx::column<Users, "username", std::string> username;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "created_at", std::string> created_at;
    
    // Primary key constraint
    relx::primary_key<&Users::id> pk;
    
    // Unique constraint
    relx::unique_constraint<&Users::email> unique_email;
};
```

Key points:
- A table is represented as a struct with a static `table_name` field
- Each column is defined using the `relx::column<TableType, Name, Type>` template
- The struct should also contain any constraints that apply to the table

## Column Types

relx automatically maps C++ types to SQL types:

| C++ Type | SQL Type |
|----------|----------|
| `int` | `INTEGER` |
| `double` | `REAL` |
| `std::string` | `TEXT` |
| `bool` | `BOOLEAN` (0 or 1) |
| `std::optional<T>` | SQL type of T, but allows NULL |

Example:

```cpp
struct Products {
    static constexpr auto table_name = "products";
    
    relx::column<Products, "id", int> id;
    relx::column<Products, "name", std::string> name;
    relx::column<Products, "price", double> price;
    relx::column<Products, "is_active", bool> is_active;
    relx::column<Products, "description", std::optional<std::string>> description;
    
    relx::primary_key<&Products::id> pk;
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
    
    relx::column<Users, "id", int> id;
    relx::column<Users, "username", std::string> username;
    
    // Primary key constraint
    relx::primary_key<&Users::id> pk;
};
```

The template parameter is a pointer to the member variable that serves as the primary key.

## Foreign Keys

Foreign keys define relationships between tables:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int> id;
    // ...
    
    relx::primary_key<&Users::id> pk;
};

struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::column<Posts, "id", int> id;
    relx::column<Posts, "user_id", int> user_id;
    // ...
    
    relx::primary_key<&Posts::id> pk;
    
    // Foreign key from Posts.user_id to Users.id
    relx::foreign_key<&Posts::user_id, &Users::id> user_fk;
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
    
    relx::column<Users, "id", int> id;
    relx::column<Users, "email", std::string> email;
    // ...
    
    relx::primary_key<&Users::id> pk;
    
    // Unique constraint on email
    relx::unique_constraint<&Users::email> unique_email;
};
```

The template parameter is a pointer to the member variable that must be unique.

## Nullable Columns

To define nullable columns, use `std::optional<T>`:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "bio", std::optional<std::string>> bio;
    relx::column<Users, "last_login", std::optional<std::string>> last_login;
    // ...
    
    relx::primary_key<&Users::id> pk;
};
```

In this example, `bio` and `last_login` are nullable columns.

## Indexes

To define indexes for faster queries:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    // ...
    
    relx::primary_key<&Users::id> pk;
    
    // Regular index on name (for fast searching)
    relx::index<&Users::name> name_idx;
    
    // Unique index on email (for fast searching & enforcing uniqueness)
    relx::unique_index<&Users::email> email_idx;
};
```

## Composite Keys

relx also supports composite (multi-column) keys:

```cpp
struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    relx::column<OrderItems, "order_id", int> order_id;
    relx::column<OrderItems, "product_id", int> product_id;
    relx::column<OrderItems, "quantity", int> quantity;
    relx::column<OrderItems, "price", double> price;
    
    // Composite primary key
    relx::composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
};
```

Similarly, you can create composite foreign keys and composite unique constraints:

```cpp
struct Orders {
    static constexpr auto table_name = "orders";
    
    relx::column<Orders, "id", int> id;
    // ...
    
    relx::primary_key<&Orders::id> pk;
};

struct Products {
    static constexpr auto table_name = "products";
    
    relx::column<Products, "id", int> id;
    // ...
    
    relx::primary_key<&Products::id> pk;
};

struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    relx::column<OrderItems, "order_id", int> order_id;
    relx::column<OrderItems, "product_id", int> product_id;
    // ...
    
    // Composite primary key
    relx::composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
    
    // Foreign keys to parent tables
    relx::foreign_key<&OrderItems::order_id, &Orders::id> order_fk;
    relx::foreign_key<&OrderItems::product_id, &Products::id> product_fk;
};
```

## Automatic SQL Generation

relx can automatically generate SQL DDL (Data Definition Language) statements for creating tables:

```cpp
Users users;

// Generate CREATE TABLE statement
std::string create_table_sql = relx::create_table(users);

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
// Create tables
auto create_users_table = relx::create_table(users);
conn.execute(create_users_table);

auto create_posts_table = relx::create_table(posts);
conn.execute(create_posts_table);
```

struct User {
    static constexpr auto table_name = "users";
    
    relx::column<User, "id", int> id;
    relx::column<User, "username", std::string> username;
    
    relx::primary_key<&User::id> pk;
};

struct Order {
    static constexpr auto table_name = "orders";
    
    relx::column<Order, "id", int> id;
    
    relx::primary_key<&Order::id> pk;
};

struct OrderItem {
    static constexpr auto table_name = "order_items";
    
    relx::column<OrderItem, "id", int> id;
    relx::column<OrderItem, "user_id", int> user_id;
    
    relx::primary_key<&OrderItem::id> pk;
    relx::foreign_key<&OrderItem::user_id, &User::id> user_fk;
};

struct Post {
    static constexpr auto table_name = "posts";
    
    relx::column<Post, "id", int> id;
    
    relx::primary_key<&Post::id> pk;
};

struct PostTag {
    static constexpr auto table_name = "post_tags";
    
    relx::column<PostTag, "order_id", int> order_id;
    relx::column<PostTag, "product_id", int> product_id;

    relx::primary_key<&PostTag::order_id, &PostTag::product_id> pk;
}; 