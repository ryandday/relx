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

## Setup

Throughout this document, we'll use the following schema definitions:

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <string>
#include <optional>

struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"email", std::string> email;
    relx::schema::column<"age", int> age;
    relx::schema::column<"is_active", bool> is_active;
    
    relx::schema::primary_key<&Users::id> pk;
};

struct Posts {
    static constexpr auto table_name = "posts";
    relx::schema::column<"id", int> id;
    relx::schema::column<"user_id", int> user_id;
    relx::schema::column<"title", std::string> title;
    relx::schema::column<"content", std::string> content;
    
    relx::schema::primary_key<&Posts::id> pk;
    relx::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};
```

## SELECT Queries

### Basic SELECT

Selecting specific columns from a table:

```cpp
Users u;

auto query = relx::query::select(u.id, u.name, u.email)
    .from(u);

// SQL: SELECT id, name, email FROM users
```

### SELECT with WHERE Condition

Filtering rows with a WHERE clause:

```cpp
Users u;

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.age > 18);

// SQL: SELECT id, name FROM users WHERE (age > ?)
// Parameters: ["18"]
```

### SELECT with Multiple Conditions

Combining multiple conditions:

```cpp
Users u;

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.age >= 18 && u.is_active == true);

// SQL: SELECT id, name FROM users WHERE ((age >= ?) AND (is_active = ?))
// Parameters: ["18", "1"]
```

### SELECT with ORDER BY

Sorting results:

```cpp
Users u;

// Ascending order (default)
auto query1 = relx::query::select(u.id, u.name)
    .from(u)
    .order_by(u.name);

// SQL: SELECT id, name FROM users ORDER BY name ASC

// Descending order
auto query2 = relx::query::select(u.id, u.name)
    .from(u)
    .order_by(relx::query::desc(u.name));

// SQL: SELECT id, name FROM users ORDER BY name DESC

// Multiple order by clauses
auto query3 = relx::query::select(u.id, u.name)
    .from(u)
    .order_by(u.age, relx::query::desc(u.name));

// SQL: SELECT id, name FROM users ORDER BY age ASC, name DESC
```

### SELECT with LIMIT and OFFSET

Limiting results:

```cpp
Users u;

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .limit(10)
    .offset(20);

// SQL: SELECT id, name FROM users LIMIT ? OFFSET ?
// Parameters: ["10", "20"]
```

## INSERT Queries

### Basic INSERT

Inserting a single row:

```cpp
Users u;

auto query = relx::query::insert_into(u)
    .values(
        relx::query::set(u.name, "John Doe"),
        relx::query::set(u.email, "john@example.com"),
        relx::query::set(u.age, 30),
        relx::query::set(u.is_active, true)
    );

// SQL: INSERT INTO users (name, email, age, is_active) VALUES (?, ?, ?, ?)
// Parameters: ["John Doe", "john@example.com", "30", "1"]
```

### Multi-row INSERT

Inserting multiple rows:

```cpp
Users u;

auto query = relx::query::insert_into(u)
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
Users u;

auto query = relx::query::update(u)
    .set(
        relx::query::set(u.name, "New Name"),
        relx::query::set(u.email, "new@example.com")
    )
    .where(u.id == 1);

// SQL: UPDATE users SET name = ?, email = ? WHERE (id = ?)
// Parameters: ["New Name", "new@example.com", "1"]
```

### UPDATE with Expressions

Using expressions in updates:

```cpp
Posts p;

auto query = relx::query::update(p)
    .set(
        relx::query::set(p.title, "Updated Title"),
        relx::query::set_expr(p.views, p.views + 1)
    )
    .where(p.id == 1);

// SQL: UPDATE posts SET title = ?, views = views + ? WHERE (id = ?)
// Parameters: ["Updated Title", "1", "1"]
```

## DELETE Queries

### Basic DELETE

Deleting rows:

```cpp
Users u;

auto query = relx::query::delete_from(u)
    .where(u.id == 1);

// SQL: DELETE FROM users WHERE (id = ?)
// Parameters: ["1"]
```

### DELETE All Rows

Deleting all rows from a table:

```cpp
Users u;

auto query = relx::query::delete_from(u);

// SQL: DELETE FROM users
```

## JOIN Operations

### Inner JOIN

Joining two tables:

```cpp
Users u;
Posts p;

auto query = relx::query::select(u.name, p.title)
    .from(u)
    .join(p, relx::query::on(u.id == p.user_id));

// SQL: SELECT name, title FROM users JOIN posts ON (id = user_id)
```

### Multiple JOINs

Joining multiple tables:

```cpp
Users u;
Posts p;
Comments c;

auto query = relx::query::select(u.name, p.title, c.content)
    .from(u)
    .join(p, relx::query::on(u.id == p.user_id))
    .join(c, relx::query::on(p.id == c.post_id));

// SQL: SELECT name, title, content FROM users 
//      JOIN posts ON (id = user_id)
//      JOIN comments ON (id = post_id)
```

### LEFT JOIN

Including rows from the left table that have no matches:

```cpp
Users u;
Posts p;

auto query = relx::query::select(u.name, p.title)
    .from(u)
    .left_join(p, relx::query::on(u.id == p.user_id));

// SQL: SELECT name, title FROM users LEFT JOIN posts ON (id = user_id)
```

## WHERE Conditions

### Comparison Operators

relx supports all standard comparison operators:

```cpp
Users u;

// Equal
auto q1 = relx::query::select(u.id, u.name).from(u).where(u.age == 30);
// SQL: WHERE (age = ?)

// Not equal
auto q2 = relx::query::select(u.id, u.name).from(u).where(u.age != 30);
// SQL: WHERE (age != ?)

// Greater than
auto q3 = relx::query::select(u.id, u.name).from(u).where(u.age > 30);
// SQL: WHERE (age > ?)

// Greater than or equal
auto q4 = relx::query::select(u.id, u.name).from(u).where(u.age >= 30);
// SQL: WHERE (age >= ?)

// Less than
auto q5 = relx::query::select(u.id, u.name).from(u).where(u.age < 30);
// SQL: WHERE (age < ?)

// Less than or equal
auto q6 = relx::query::select(u.id, u.name).from(u).where(u.age <= 30);
// SQL: WHERE (age <= ?)
```

### Logical Operators

Combining conditions with AND, OR, and NOT:

```cpp
Users u;

// AND
auto q1 = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.age > 18 && u.is_active == true);
// SQL: WHERE ((age > ?) AND (is_active = ?))

// OR
auto q2 = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.age < 18 || u.age > 65);
// SQL: WHERE ((age < ?) OR (age > ?))

// NOT
auto q3 = relx::query::select(u.id, u.name)
    .from(u)
    .where(!(u.age >= 18 && u.age <= 65));
// SQL: WHERE (NOT ((age >= ?) AND (age <= ?)))

// Complex condition
auto q4 = relx::query::select(u.id, u.name)
    .from(u)
    .where((u.age < 18 || u.age > 65) && u.is_active == true);
// SQL: WHERE (((age < ?) OR (age > ?)) AND (is_active = ?))
```

### IN Operator

Checking if a value is in a set:

```cpp
Users u;

// Using a vector
std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
auto q1 = relx::query::select(u.id, u.email)
    .from(u)
    .where(relx::query::in(u.name, names));
// SQL: WHERE name IN (?, ?, ?)

// Using direct values
auto q2 = relx::query::select(u.id, u.email)
    .from(u)
    .where(relx::query::in(u.age, {18, 21, 25, 30}));
// SQL: WHERE age IN (?, ?, ?, ?)
```

### LIKE Operator

Pattern matching with LIKE:

```cpp
Users u;

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .where(relx::query::like(u.email, "%@example.com"));

// SQL: SELECT id, name FROM users WHERE email LIKE ?
// Parameters: ["%@example.com"]
```

### IS NULL / IS NOT NULL

Checking for NULL values:

```cpp
Users u;

// Using is_null()
auto q1 = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.email.is_null());
// SQL: WHERE (email IS NULL)

// Using is_not_null()
auto q2 = relx::query::select(u.id, u.name)
    .from(u)
    .where(u.email.is_not_null());
// SQL: WHERE (email IS NOT NULL)
```

## ORDER BY and LIMIT

### Multiple ORDER BY Clauses

Sorting by multiple columns:

```cpp
Users u;

auto query = relx::query::select(u.id, u.name, u.age)
    .from(u)
    .order_by(
        u.age,                      // ASC by default
        relx::query::desc(u.name) // DESC explicitly
    );

// SQL: SELECT id, name, age FROM users ORDER BY age ASC, name DESC
```

### LIMIT with OFFSET

Pagination:

```cpp
Users u;

// Page size = 10, page number = 3
int page_size = 10;
int page_number = 3;
int offset = (page_number - 1) * page_size;

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .order_by(u.id)
    .limit(page_size)
    .offset(offset);

// SQL: SELECT id, name FROM users ORDER BY id ASC LIMIT ? OFFSET ?
// Parameters: ["10", "20"]
```

## Aggregation Functions

### Basic Aggregates

Using count, sum, avg, min, and max:

```cpp
Users u;
Posts p;

// COUNT
auto q1 = relx::query::select_expr(
    relx::query::count_all()
).from(u);
// SQL: SELECT COUNT(*) FROM users

// COUNT with column
auto q2 = relx::query::select_expr(
    relx::query::count(u.id)
).from(u);
// SQL: SELECT COUNT(id) FROM users

// SUM
auto q3 = relx::query::select_expr(
    relx::query::sum(p.views)
).from(p);
// SQL: SELECT SUM(views) FROM posts

// AVG
auto q4 = relx::query::select_expr(
    relx::query::avg(u.age)
).from(u);
// SQL: SELECT AVG(age) FROM users

// MIN and MAX
auto q5 = relx::query::select_expr(
    relx::query::min(u.age),
    relx::query::max(u.age)
).from(u);
// SQL: SELECT MIN(age), MAX(age) FROM users
```

### Alias for Aggregates

Using aliases for readable result columns:

```cpp
Users u;

auto query = relx::query::select_expr(
    relx::query::as(relx::query::count_all(), "user_count"),
    relx::query::as(relx::query::avg(u.age), "average_age")
).from(u);

// SQL: SELECT COUNT(*) AS user_count, AVG(age) AS average_age FROM users
```

### GROUP BY

Grouping results:

```cpp
Users u;
Posts p;

auto query = relx::query::select_expr(
    u.is_active,
    relx::query::as(relx::query::count(u.id), "user_count"),
    relx::query::as(relx::query::avg(u.age), "average_age")
).from(u)
 .group_by(u.is_active);

// SQL: SELECT is_active, COUNT(id) AS user_count, AVG(age) AS average_age 
//      FROM users 
//      GROUP BY is_active
```

### HAVING

Filtering grouped results:

```cpp
Users u;
Posts p;

auto query = relx::query::select_expr(
    u.id,
    u.name,
    relx::query::as(relx::query::count(p.id), "post_count")
).from(u)
 .join(p, relx::query::on(u.id == p.user_id))
 .group_by(u.id, u.name)
 .having(relx::query::count(p.id) > 5);

// SQL: SELECT id, name, COUNT(id) AS post_count 
//      FROM users 
//      JOIN posts ON (id = user_id) 
//      GROUP BY id, name 
//      HAVING (COUNT(id) > ?)
// Parameters: ["5"]
```

## Subqueries

### Subquery in WHERE Clause

Using a subquery for filtering:

```cpp
Users u;
Posts p;

// Find users who have at least one post
auto subquery = relx::query::select(p.user_id)
    .from(p);

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .where(relx::query::in(u.id, subquery));

// SQL: SELECT id, name FROM users WHERE id IN (SELECT user_id FROM posts)
```

### Correlated Subquery

A subquery that references the outer query:

```cpp
Users u;
Posts p;

// Find users who have more than 3 posts
auto correlated_subquery = relx::query::select_expr(
    relx::query::count(p.id)
).from(p)
 .where(p.user_id == u.id);

auto query = relx::query::select(u.id, u.name)
    .from(u)
    .where(correlated_subquery > 3);

// SQL: SELECT id, name FROM users WHERE (SELECT COUNT(id) FROM posts WHERE (user_id = id)) > ?
// Parameters: ["3"]
```

## Case Expressions

### Simple CASE

Conditional expressions:

```cpp
Users u;

auto query = relx::query::select_expr(
    u.id,
    u.name,
    relx::query::as(
        relx::query::case_()
            .when(u.age < 18, "Minor")
            .when(u.age < 65, "Adult")
            .else_("Senior")
            .build(),
        "age_group"
    )
).from(u);

// SQL: SELECT id, name, 
//      CASE WHEN (age < ?) THEN ? WHEN (age < ?) THEN ? ELSE ? END AS age_group 
//      FROM users
// Parameters: ["18", "Minor", "65", "Adult", "Senior"]
```

### CASE with Expressions

Using expressions in CASE statements:

```cpp
Users u;
Posts p;

auto query = relx::query::select_expr(
    u.id,
    u.name,
    relx::query::as(
        relx::query::case_()
            .when(relx::query::count(p.id) == 0, "No posts")
            .when(relx::query::count(p.id) < 5, "Few posts")
            .else_("Many posts")
            .build(),
        "post_status"
    )
).from(u)
 .left_join(p, relx::query::on(u.id == p.user_id))
 .group_by(u.id, u.name);

// SQL: SELECT id, name, 
//      CASE WHEN (COUNT(id) = ?) THEN ? WHEN (COUNT(id) < ?) THEN ? ELSE ? END AS post_status 
//      FROM users 
//      LEFT JOIN posts ON (id = user_id) 
//      GROUP BY id, name
// Parameters: ["0", "No posts", "5", "Few posts", "Many posts"]
``` 