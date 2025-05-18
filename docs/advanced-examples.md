# Advanced Query Examples

This document provides more complex examples of using relx for advanced query scenarios.

## Table of Contents

- [Complex Joins](#complex-joins)
- [Aggregate Functions](#aggregate-functions)
- [Subqueries](#subqueries)
- [Case Expressions](#case-expressions)
- [Working with Results](#working-with-results)
- [Transactions](#transactions)

## Schema Setup

We'll use the following schema for our examples:

```cpp
#include <relx/schema.hpp>
#include <string>
#include <optional>

// Users table
struct Users {
    static constexpr auto table_name = "users";
    relx::column<"id", int> id;
    relx::column<"name", std::string> name;
    relx::column<"email", std::string> email;
    relx::column<"age", int> age;
    relx::column<"is_active", bool> is_active;
    relx::column<"department_id", int> department_id;
    relx::column<"bio", std::optional<std::string>> bio;
    
    relx::primary_key<&Users::id> pk;
    relx::unique_constraint<&Users::email> unique_email;
};

// Posts table
struct Posts {
    static constexpr auto table_name = "posts";
    relx::column<"id", int> id;
    relx::column<"user_id", int> user_id;
    relx::column<"title", std::string> title;
    relx::column<"content", std::string> content;
    relx::column<"views", int> views;
    relx::column<"created_at", std::string> created_at;
    
    relx::primary_key<&Posts::id> pk;
    relx::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

// Comments table
struct Comments {
    static constexpr auto table_name = "comments";
    relx::column<"id", int> id;
    relx::column<"post_id", int> post_id;
    relx::column<"user_id", int> user_id; 
    relx::column<"content", std::string> content;
    relx::column<"created_at", std::string> created_at;
    
    relx::primary_key<&Comments::id> pk;
    relx::foreign_key<&Comments::post_id, &Posts::id> post_fk;
    relx::foreign_key<&Comments::user_id, &Users::id> user_fk;
};

// Departments table
struct Departments {
    static constexpr auto table_name = "departments";
    relx::column<"id", int> id;
    relx::column<"name", std::string> name;
    relx::column<"budget", double> budget;
    
    relx::primary_key<&Departments::id> pk;
};
```

## Complex Joins

### Multi-table Join Example

Joining multiple tables to get users, their posts, and comments:

```cpp
Users u;
Posts p;
Comments c;

auto query = relx::select(
    u.name, p.title, c.content
).from(u)
 .join(p, relx::on(u.id == p.user_id))
 .join(c, relx::on(p.id == c.post_id));

// Generated SQL:
// SELECT users.name, posts.title, comments.content 
// FROM users 
// JOIN posts ON (users.id = posts.user_id) 
// JOIN comments ON (posts.id = comments.post_id)
```

### Left Join with NULL Handling

Finding all users and their posts, including users without posts:

```cpp
Users u;
Posts p;

auto query = relx::select(
    u.name,
    p.title,
    relx::as(
        relx::case_()
            .when(p.id.is_null(), "No posts")
            .else_("Has posts")
            .build(),
        "post_status"
    )
).from(u)
 .left_join(p, relx::on(u.id == p.user_id));

// Generated SQL:
// SELECT users.name, posts.title, 
//   CASE WHEN (posts.id IS NULL) THEN ? ELSE ? END AS post_status 
// FROM users 
// LEFT JOIN posts ON (users.id = posts.user_id)
```

## Aggregate Functions

### Group By with Aggregates

Count posts per user:

```cpp
Users u;
Posts p;

auto query = relx::select_expr(
    u.id,
    u.name,
    relx::as(relx::count(p.id), "post_count")
).from(u)
 .join(p, relx::on(u.id == p.user_id))
 .group_by(u.id, u.name);

// Generated SQL:
// SELECT users.id, users.name, COUNT(posts.id) AS post_count 
// FROM users 
// JOIN posts ON (users.id = posts.user_id) 
// GROUP BY users.id, users.name
```

### Having Clause

Find users with more than 5 posts:

```cpp
Users u;
Posts p;

auto query = relx::select_expr(
    u.id,
    u.name,
    relx::as(relx::count(p.id), "post_count")
).from(u)
 .join(p, relx::on(u.id == p.user_id))
 .group_by(u.id, u.name)
 .having(relx::count(p.id) > 5);

// Generated SQL:
// SELECT users.id, users.name, COUNT(posts.id) AS post_count 
// FROM users 
// JOIN posts ON (users.id = posts.user_id) 
// GROUP BY users.id, users.name 
// HAVING (COUNT(posts.id) > ?)
```

### Multiple Aggregates

Compute statistics for departments:

```cpp
Users u;
Posts p;
Departments d;

auto query = relx::select_expr(
    d.name,
    relx::as(relx::count(u.id), "user_count"),
    relx::as(relx::count(p.id), "post_count"),
    relx::as(relx::sum(p.views), "total_views")
).from(d)
 .join(u, relx::on(d.id == u.department_id))
 .join(p, relx::on(u.id == p.user_id))
 .group_by(d.name);

// Generated SQL:
// SELECT departments.name, 
//   COUNT(users.id) AS user_count, 
//   COUNT(posts.id) AS post_count, 
//   SUM(posts.views) AS total_views 
// FROM departments 
// JOIN users ON (departments.id = users.department_id) 
// JOIN posts ON (users.id = posts.user_id) 
// GROUP BY departments.name
```

## Subqueries

### IN Clause with Subquery

Find users who have written posts with more than 100 views:

```cpp
Users u;
Posts p;

auto subquery = relx::select(p.user_id)
    .from(p)
    .where(p.views > 100);

auto query = relx::select(u.id, u.name)
    .from(u)
    .where(relx::in(u.id, subquery));

// Generated SQL:
// SELECT users.id, users.name 
// FROM users 
// WHERE users.id IN (
//   SELECT posts.user_id 
//   FROM posts 
//   WHERE (posts.views > ?)
// )
```

### Correlated Subquery

Find users whose average post views exceed the overall average:

```cpp
Users u;
Posts p;

auto avg_views_subquery = relx::select_expr(
    relx::avg(p.views)
).from(p);

auto user_avg_views_subquery = relx::select_expr(
    relx::avg(p.views)
).from(p)
 .where(p.user_id == u.id);

auto query = relx::select(u.id, u.name)
    .from(u)
    .where(user_avg_views_subquery > avg_views_subquery);

// Generated SQL:
// SELECT users.id, users.name 
// FROM users 
// WHERE (
//   SELECT AVG(posts.views) 
//   FROM posts 
//   WHERE (posts.user_id = users.id)
// ) > (
//   SELECT AVG(posts.views) 
//   FROM posts
// )
```

## Case Expressions

### Simple CASE

Categorize users by age:

```cpp
Users u;

auto query = relx::select_expr(
    u.id,
    u.name,
    relx::as(
        relx::case_()
            .when(u.age < 18, "Minor")
            .when(u.age < 65, "Adult")
            .else_("Senior")
            .build(),
        "age_group"
    )
).from(u);

// Generated SQL:
// SELECT users.id, users.name, 
//   CASE 
//     WHEN (users.age < ?) THEN ? 
//     WHEN (users.age < ?) THEN ? 
//     ELSE ? 
//   END AS age_group 
// FROM users
```

### CASE with Aggregates

Count users in each age group:

```cpp
Users u;

auto query = relx::select_expr(
    relx::as(
        relx::case_()
            .when(u.age < 18, "Minor")
            .when(u.age < 65, "Adult")
            .else_("Senior")
            .build(),
        "age_group"
    ),
    relx::as(relx::count_all(), "count")
).from(u)
 .group_by(
    relx::case_()
        .when(u.age < 18, "Minor")
        .when(u.age < 65, "Adult")
        .else_("Senior")
        .build()
 );

// Generated SQL:
// SELECT 
//   CASE 
//     WHEN (users.age < ?) THEN ? 
//     WHEN (users.age < ?) THEN ? 
//     ELSE ? 
//   END AS age_group, 
//   COUNT(*) AS count 
// FROM users 
// GROUP BY 
//   CASE 
//     WHEN (users.age < ?) THEN ? 
//     WHEN (users.age < ?) THEN ? 
//     ELSE ? 
//   END
```

### Handling NULL Values

Working with optional columns:

```cpp
Users u;

auto query = relx::select(u.id, u.name, u.bio)
    .from(u);

auto result = conn.execute(query);
if (result) {
    for (const auto& row : *result) {
        auto id = row.get<int>("id");
        auto name = row.get<std::string>("name");
        auto bio = row.get<std::optional<std::string>>("bio");
        
        if (id && name) {
            std::print("User: {} - {}", *id, *name);
            if (bio && *bio) {
                std::print(" - Bio: {}", **bio);
            } else {
                std::print(" - No bio available");
            }
            std::println("");
        }
    }
}
```

## Transactions

### Basic Transaction

Execute multiple queries in a transaction:

```cpp
Users u;
Posts p;

// Start a transaction
auto transaction = conn.begin_transaction();

// Insert a user
auto insert_user = relx::insert_into(u)
    .values(
        relx::set(u.id, 1),
        relx::set(u.name, "John Doe"),
        relx::set(u.email, "john@example.com"),
        relx::set(u.age, 30),
        relx::set(u.is_active, true),
        relx::set(u.department_id, 1)
    );

auto user_result = conn.execute(insert_user);
if (!user_result) {
    transaction.rollback();
    std::print("Failed to insert user: {}", user_result.error().message);
    return;
}

// Insert a post for the user
auto insert_post = relx::insert_into(p)
    .values(
        relx::set(p.id, 1),
        relx::set(p.user_id, 1),
        relx::set(p.title, "My First Post"),
        relx::set(p.content, "This is my first post."),
        relx::set(p.views, 0),
        relx::set(p.created_at, "2023-01-01")
    );

auto post_result = conn.execute(insert_post);
if (!post_result) {
    transaction.rollback();
    std::print("Failed to insert post: {}", post_result.error().message);
    return;
}

// Commit the transaction
transaction.commit();
```

### Transaction with RAII

Using RAII for transaction management:

```cpp
void insert_data(relx::Connection& conn) {
    Users u;
    Posts p;
    
    // Create an RAII transaction
    relx::Transaction transaction(conn);
    
    try {
        // Insert a user
        auto insert_user = relx::insert_into(u)
            .values(
                relx::set(u.id, 1),
                relx::set(u.name, "John Doe"),
                relx::set(u.email, "john@example.com"),
                relx::set(u.age, 30),
                relx::set(u.is_active, true),
                relx::set(u.department_id, 1)
            );
        
        conn.execute(insert_user);
        
        // Insert a post
        auto insert_post = relx::insert_into(p)
            .values(
                relx::set(p.id, 1),
                relx::set(p.user_id, 1),
                relx::set(p.title, "My First Post"),
                relx::set(p.content, "This is my first post."),
                relx::set(p.views, 0),
                relx::set(p.created_at, "2023-01-01")
            );
        
        conn.execute(insert_post);
        
        // If we reach here without exceptions, commit the transaction
        transaction.commit();
    } catch (const std::exception& e) {
        // Transaction will automatically rollback if not committed
        std::print("Error: {}", e.what());
        throw; // Re-throw the exception
    }
} 