# Query Building in relx

relx provides a fluent API for building SQL queries in a type-safe manner. This document explains how to construct various types of queries.

## Table of Contents
- [SELECT Queries](#select-queries)
- [INSERT Queries](#insert-queries)
- [UPDATE Queries](#update-queries)
- [DELETE Queries](#delete-queries)
- [JOIN Operations](#join-operations)
- [WHERE Conditions](#where-conditions)
- [ORDER BY and LIMIT](#order-by-and-limit)
- [Aggregation Functions](#aggregation-functions)
- [Subqueries](#subqueries)
- [Case Expressions](#case-expressions)
- [PostgreSQL-Specific Features](#postgresql-specific-features)

## Setup

Throughout this document, we'll use the following schema definitions:

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <string>
#include <optional>

// clang-format off
struct [[=relx::table("users")]] Users {
    [[=relx::ann::pk]] int id;
    std::string name;
    std::string email;
    int age;
    bool is_active;
    std::optional<std::string> bio;
};

struct [[=relx::table("posts")]] Posts {
    [[=relx::ann::pk]] int id;
    [[=relx::ann::fk<^^Users::id>]] int user_id;
    std::string title;
    std::string content;
    int views;
};

struct [[=relx::table("comments")]] Comments {
    [[=relx::ann::pk]] int id;
    [[=relx::ann::fk<^^Posts::id>]] int post_id;
    std::string content;
};
// clang-format on

// The table objects the examples below use; define each once, next to its struct
inline constexpr auto u = relx::t<Users>;
inline constexpr auto p = relx::t<Posts>;
inline constexpr auto c = relx::t<Comments>;
```

## SELECT Queries

### Basic SELECT

Selecting specific columns from a table:

```cpp
auto query = relx::select(u.id, u.name, u.email)
    .from(u);

// SQL: SELECT users.id, users.name, users.email FROM users
```

### SELECT with WHERE Condition

Filtering rows with a WHERE clause:

```cpp
auto query = relx::select(u.id, u.name)
    .from(u)
    .where(u.age > 18);

// SQL: SELECT users.id, users.name FROM users WHERE (users.age > ?)
// Parameters: ["18"]
```

### SELECT with Multiple Conditions

Combining multiple conditions:

```cpp
auto query = relx::select(u.id, u.name)
    .from(u)
    .where(u.age >= 18 && u.is_active == true);

// SQL: SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND (users.is_active = ?))
// Parameters: ["18", "true"]
```

### SELECT with ORDER BY

Sorting results:

```cpp
// Ascending order (default)
auto query1 = relx::select(u.id, u.name)
    .from(u)
    .order_by(u.name);

// SQL: SELECT users.id, users.name FROM users ORDER BY users.name ASC

// Descending order
auto query2 = relx::select(u.id, u.name)
    .from(u)
    .order_by(relx::desc(u.name));

// SQL: SELECT users.id, users.name FROM users ORDER BY users.name DESC

// Multiple order by clauses
auto query3 = relx::select(u.id, u.name)
    .from(u)
    .order_by(relx::asc(u.age), relx::desc(u.name));

// SQL: SELECT users.id, users.name FROM users ORDER BY users.age ASC, users.name DESC
```

### SELECT with LIMIT and OFFSET

Limiting results:

```cpp
auto query = relx::select(u.id, u.name)
    .from(u)
    .limit(10)
    .offset(20);

// SQL: SELECT users.id, users.name FROM users LIMIT ? OFFSET ?
// Parameters: ["10", "20"]
```

## INSERT Queries

### Basic INSERT

Inserting a single row:

```cpp
auto query = relx::insert_into(u)
    .columns(u.name, u.email, u.age, u.is_active)
    .values("John Doe", "john@example.com", 30, true);

// SQL: INSERT INTO users (name, email, age, is_active) VALUES (?, ?, ?, ?)
// Parameters: ["John Doe", "john@example.com", "30", "true"]
```

### Multi-row INSERT

Inserting multiple rows:

```cpp
auto query = relx::insert_into(u)
    .columns(u.name, u.email, u.age)
    .values("John Doe", "john@example.com", 30)
    .values("Jane Smith", "jane@example.com", 25);

// SQL: INSERT INTO users (name, email, age) VALUES (?, ?, ?), (?, ?, ?)
// Parameters: ["John Doe", "john@example.com", "30", "Jane Smith", "jane@example.com", "25"]
```

## UPDATE Queries

### Basic UPDATE

Updating rows:

```cpp
auto query = relx::update(u)
    .set(u.name, "New Name")
    .set(u.email, "new@example.com")
    .where(u.id == 1);

// SQL: UPDATE users SET name = ?, email = ? WHERE (users.id = ?)
// Parameters: ["New Name", "new@example.com", "1"]
```

## DELETE Queries

### Basic DELETE

Deleting rows:

```cpp
auto query = relx::delete_from(u)
    .where(u.id == 1);

// SQL: DELETE FROM users WHERE (users.id = ?)
// Parameters: ["1"]
```

### DELETE All Rows

Deleting all rows from a table:

```cpp
auto query = relx::delete_from(u);

// SQL: DELETE FROM users
```

## JOIN Operations

### Inner JOIN

Joining two tables:

```cpp
auto query = relx::select(u.name, p.title)
    .from(u)
    .join(p, relx::on(u.id == p.user_id));

// SQL: SELECT users.name, posts.title FROM users JOIN posts ON (users.id = posts.user_id)
```

### Multiple JOINs

Joining multiple tables:

```cpp
auto query = relx::select(u.name, p.title, c.content)
    .from(u)
    .join(p, relx::on(u.id == p.user_id))
    .join(c, relx::on(p.id == c.post_id));

// SQL: SELECT users.name, posts.title, comments.content 
// FROM users 
// JOIN posts ON (users.id = posts.user_id) 
// JOIN comments ON (posts.id = comments.post_id)
```

### LEFT JOIN

Including rows from the left table even when there are no matches in the right table:

```cpp
auto query = relx::select(u.name, p.title)
    .from(u)
    .left_join(p, relx::on(u.id == p.user_id));

// SQL: SELECT users.name, posts.title 
// FROM users 
// LEFT JOIN posts ON (users.id = posts.user_id)
```

### RIGHT JOIN

Including rows from the right table even when there are no matches in the left table:

```cpp
auto query = relx::select(u.name, p.title)
    .from(u)
    .right_join(p, relx::on(u.id == p.user_id));

// SQL: SELECT users.name, posts.title 
// FROM users 
// RIGHT JOIN posts ON (users.id = posts.user_id)
```

## WHERE Conditions

### Basic Conditions

```cpp
// Equality
auto query1 = relx::select(u.id, u.name)
    .from(u)
    .where(u.id == 5);

// SQL: SELECT users.id, users.name FROM users WHERE (users.id = ?)

// Inequality
auto query2 = relx::select(u.id, u.name)
    .from(u)
    .where(u.age != 30);

// SQL: SELECT users.id, users.name FROM users WHERE (users.age != ?)

// Comparison operators
auto query3 = relx::select(u.id, u.name)
    .from(u)
    .where(u.age > 18);

// SQL: SELECT users.id, users.name FROM users WHERE (users.age > ?)
```

### Logical Operators

Combining conditions with AND/OR:

```cpp
// AND
auto query1 = relx::select(u.id, u.name)
    .from(u)
    .where(u.age >= 18 && u.age <= 65);

// SQL: SELECT users.id, users.name FROM users WHERE ((users.age >= ?) AND (users.age <= ?))

// OR
auto query2 = relx::select(u.id, u.name)
    .from(u)
    .where(u.age < 18 || u.age > 65);

// SQL: SELECT users.id, users.name FROM users WHERE ((users.age < ?) OR (users.age > ?))

// Complex condition
auto query3 = relx::select(u.id, u.name)
    .from(u)
    .where(
        (u.age >= 18 && u.is_active == true) ||
        (u.email.like("%admin%"))
    );

// SQL: SELECT users.id, users.name
// FROM users
// WHERE (((users.age >= ?) AND (users.is_active = ?)) OR users.email LIKE ?)
```

### Pattern Matching

Using LIKE for pattern matching:

```cpp
auto query = relx::select(u.id, u.name)
    .from(u)
    .where(u.email.like("%gmail.com"));

// SQL: SELECT users.id, users.name FROM users WHERE users.email LIKE ?
// Parameters: ["%gmail.com"]
```

### IN Operator

Matching against a list of values:

```cpp
// IN binds one parameter per value; values keep their C++ types
auto query = relx::select(u.id, u.name)
    .from(u)
    .where(relx::in(u.id, std::vector<int>{1, 2, 3, 4, 5}));

// SQL: SELECT users.id, users.name FROM users WHERE users.id IN (?, ?, ?, ?, ?)
// Parameters: 1, 2, 3, 4, 5 (typed int4 binds)
```

`= ANY(?)` is usually the better tool: the values keep their C++ types, the whole list travels as a
single array parameter, and the SQL text does not depend on the list length — so the query stays
preparable and an empty list is valid (`IN ()` would be a syntax error).

```cpp
auto query = relx::select(u.id, u.name)
    .from(u)
    .where(relx::in_any(u.id, std::vector<int>{1, 2, 3}));

// SQL: SELECT users.id, users.name FROM users WHERE users.id = ANY(?)
```

### NULL Checking

Checking for NULL values:

```cpp
// IS NULL
auto query1 = relx::select(u.id, u.name)
    .from(u)
    .where(u.bio.is_null());

// SQL: SELECT users.id, users.name FROM users WHERE users.bio IS NULL

// IS NOT NULL
auto query2 = relx::select(u.id, u.name)
    .from(u)
    .where(u.bio.is_not_null());

// SQL: SELECT users.id, users.name FROM users WHERE users.bio IS NOT NULL
```

## ORDER BY and LIMIT

### Multiple ORDER BY Clauses

Sorting by multiple columns:

```cpp
auto query = relx::select(u.id, u.name, u.age)
    .from(u)
    .order_by(
        relx::asc(u.age),
        relx::desc(u.name)
    );

// SQL: SELECT users.id, users.name, users.age FROM users ORDER BY users.age ASC, users.name DESC
```

### LIMIT with OFFSET

Pagination:

```cpp
// Page size = 10, page number = 3
int page_size = 10;
int page_number = 3;
int offset = (page_number - 1) * page_size;

auto query = relx::select(u.id, u.name)
    .from(u)
    .order_by(u.id)
    .limit(page_size)
    .offset(offset);

// SQL: SELECT users.id, users.name FROM users ORDER BY users.id ASC LIMIT ? OFFSET ?
// Parameters: ["10", "20"]
```

## Aggregation Functions

### Basic Aggregates

Using count, sum, avg, min, and max:

```cpp
// COUNT
auto q1 = relx::select_expr(
    relx::count_all()
).from(u);
// SQL: SELECT COUNT(*) FROM users

// COUNT with column
auto q2 = relx::select_expr(
    relx::count(u.id)
).from(u);
// SQL: SELECT COUNT(users.id) FROM users

// SUM
auto q3 = relx::select_expr(
    relx::sum(p.views)
).from(p);
// SQL: SELECT SUM(posts.views) FROM posts

// AVG
auto q4 = relx::select_expr(
    relx::avg(u.age)
).from(u);
// SQL: SELECT AVG(users.age) FROM users

// MIN and MAX
auto q5 = relx::select_expr(
    relx::min(u.age),
    relx::max(u.age)
).from(u);
// SQL: SELECT MIN(users.age), MAX(users.age) FROM users
```

### Alias for Aggregates

Using aliases for readable result columns:

```cpp
auto query = relx::select_expr(
    relx::as(relx::count_all(), "user_count"),
    relx::as(relx::avg(u.age), "average_age")
).from(u);

// SQL: SELECT COUNT(*) AS user_count, AVG(users.age) AS average_age FROM users
```

### GROUP BY

Grouping results:

```cpp
auto query = relx::select_expr(
    u.is_active,
    relx::as(relx::count(u.id), "user_count"),
    relx::as(relx::avg(u.age), "average_age")
).from(u)
 .group_by(u.is_active);

// SQL: SELECT users.is_active, COUNT(users.id) AS user_count, AVG(users.age) AS average_age
//      FROM users
//      GROUP BY users.is_active
```

### HAVING

Filtering grouped results:

```cpp
auto query = relx::select_expr(
    u.id,
    u.name,
    relx::as(relx::count(p.id), "post_count")
).from(u)
 .join(p, relx::on(u.id == p.user_id))
 .group_by(u.id, u.name)
 .having(relx::count(p.id) > 5);

// SQL: SELECT users.id, users.name, COUNT(posts.id) AS post_count
//      FROM users
//      JOIN posts ON (users.id = posts.user_id)
//      GROUP BY users.id, users.name
//      HAVING (COUNT(posts.id) > ?)
// Parameters: ["5"]
```

## Subqueries

A SELECT query nests inside a WHERE clause. Its SQL is inlined in parentheses and its bind
parameters travel with the outer query's, in order.

```cpp
// IN (SELECT ...)
auto sub = relx::select(p.user_id).from(p).where(p.views > 100);

auto query = relx::select(u.name)
    .from(u)
    .where(relx::in(u.id, sub));

// SQL: SELECT users.name FROM users WHERE users.id IN
//      (SELECT posts.user_id FROM posts WHERE (posts.views > ?))
```

`EXISTS` / `NOT EXISTS` take the subquery directly; correlate by referencing the outer
table's columns inside the subquery:

```cpp
auto sub = relx::select(p.id)
    .from(p)
    .where(p.user_id == u.id && p.views > 50);

auto query = relx::select(u.name)
    .from(u)
    .where(relx::exists(sub));

// SQL: SELECT users.name FROM users WHERE EXISTS
//      (SELECT posts.id FROM posts WHERE ((posts.user_id = users.id) AND (posts.views > ?)))
```

A SELECT can also feed an INSERT:

```cpp
auto select_query = relx::select(u.id, u.name)
    .from(u)
    .where(u.is_active == true);

auto query = relx::insert_into(p)
    .columns(p.user_id, p.title)
    .select(select_query);

// SQL: INSERT INTO posts (user_id, title) SELECT users.id, users.name FROM users WHERE (users.is_active = ?)
```

## Case Expressions

### Simple CASE

Conditional expressions:

```cpp
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

// SQL: SELECT users.id, users.name,
//      CASE WHEN (users.age < ?) THEN ? WHEN (users.age < ?) THEN ? ELSE ? END AS age_group
//      FROM users
// Parameters: ["18", "Minor", "65", "Adult", "Senior"]
```

### CASE with Expressions

Using expressions in CASE statements:

```cpp
auto query = relx::select_expr(
    u.id,
    u.name,
    relx::as(
        relx::case_()
            .when(relx::count(p.id) == 0, "No posts")
            .when(relx::count(p.id) < 5, "Few posts")
            .else_("Many posts")
            .build(),
        "post_status"
    )
).from(u)
 .left_join(p, relx::on(u.id == p.user_id))
 .group_by(u.id, u.name);

// SQL: SELECT users.id, users.name,
//      CASE WHEN (COUNT(posts.id) = ?) THEN ? WHEN (COUNT(posts.id) < ?) THEN ? ELSE ? END AS post_status
//      FROM users
//      LEFT JOIN posts ON (users.id = posts.user_id)
//      GROUP BY users.id, users.name
// Parameters: ["0", "No posts", "5", "Few posts", "Many posts"]
```

## PostgreSQL-Specific Features

relx provides support for several PostgreSQL-specific features that enhance your SQL capabilities:

### RETURNING Clause

PostgreSQL allows getting back data from modified rows using the RETURNING clause:

```cpp
// INSERT with RETURNING
auto insert_query = relx::insert_into(u)
    .columns(u.name, u.email)
    .values("John Doe", "john@example.com")
    .returning(u.id);  // Get the assigned ID

// SQL: INSERT INTO users (name, email) VALUES (?, ?) RETURNING users.id
// Parameters: ["John Doe", "john@example.com"]

// UPDATE with RETURNING
auto update_query = relx::update(u)
    .set(u.name, "Updated Name")
    .where(u.id == 1)
    .returning(u.id, u.name);  // Get the updated rows

// SQL: UPDATE users SET name = ? WHERE (users.id = ?) RETURNING users.id, users.name
// Parameters: ["Updated Name", "1"]

// DELETE with RETURNING
auto delete_query = relx::delete_from(u)
    .where(u.id == 1)
    .returning(u.id, u.name);  // Get the deleted rows

// SQL: DELETE FROM users WHERE (users.id = ?) RETURNING users.id, users.name
// Parameters: ["1"]
```