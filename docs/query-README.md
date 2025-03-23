# SQLlib Query System

The SQLlib query system is a modern C++ library designed to provide a type-safe, compile-time checked SQL query building experience. This document explains how the query functionality works, starting with basic concepts and building up to more complex features.

## Table of Contents

1. [Core Concepts](#core-concepts)
2. [Basic Building Blocks](#basic-building-blocks)
3. [Column Expressions](#column-expressions)
4. [Value Handling](#value-handling)
5. [Conditions](#conditions)
6. [Functions and Expressions](#functions-and-expressions)
7. [Building Queries](#building-queries)
8. [Advanced Features](#advanced-features)
9. [Putting It All Together](#putting-it-all-together)
10. [Type System Deep Dive](#type-system-deep-dive)

## Core Concepts

The foundation of the query system is built on several key concepts that are defined as C++ concepts in the modern sense:

- **SqlExpr**: Represents any SQL expression component that can be converted to SQL and has bind parameters
- **TableType**: Represents a database table
- **ColumnType**: Represents a column in a database table
- **ConditionExpr**: Represents a condition expression used in WHERE clauses

The base class for all SQL expressions is `SqlExpression` which defines two key virtual methods:

```cpp
struct SqlExpression {
    virtual ~SqlExpression() = default;
    virtual std::string to_sql() const = 0;
    virtual std::vector<std::string> bind_params() const = 0;
};
```

These methods allow any SQL expression to:
1. Generate its SQL string representation
2. Provide a list of parameters to bind to the query

This design allows the library to build complex SQL queries by composing smaller SQL expressions, each responsible for its own SQL generation and parameter binding.

## Basic Building Blocks

### Tables

Tables are represented as C++ structs with column members and metadata like table name:

```cpp
struct users {
    static constexpr auto table_name = "users";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    // other columns...
    
    // Primary key, constraints, etc.
    sqllib::schema::primary_key<&users::id> pk;
};
```

### Columns

Columns are defined as template instances that include the column name as a string literal template parameter and the C++ type of the column:

```cpp
sqllib::schema::column<"name", std::string> name;
```

This design allows the library to know both the SQL column name and C++ type at compile time.

## Column Expressions

To use columns in queries, they need to be wrapped as expressions. The `ColumnRef` class is used to reference columns in expressions:

```cpp
template <ColumnType Column>
class ColumnRef : public ColumnExpression {
    // Implementation...
};
```

Column expressions can also be aliased using the `AliasedColumn` class:

```cpp
// Create an aliased column
auto aliased_col = sqllib::query::as(sqllib::query::to_expr(users_table.name), "user_name");
```

## Value Handling

Values are wrapped in the `Value` template class to be used in queries:

```cpp
template <typename T>
class Value : public SqlExpression {
    // Implementation...
};
```

The library provides convenience functions to create value expressions:

```cpp
// Create value expressions
auto val1 = sqllib::query::val(42);              // int
auto val2 = sqllib::query::val("some string");   // string
auto val3 = sqllib::query::val(true);            // boolean
```

Values get automatically converted to bind parameters in the generated SQL, protecting against SQL injection.

## Conditions

Conditions are expressions used in WHERE clauses and JOIN conditions. The library provides several types of conditions:

### Binary Conditions

These compare two expressions using operators:

```cpp
// Column equals value
auto condition = sqllib::query::to_expr(users.id) == sqllib::query::val(1);

// Age greater than value
auto age_condition = sqllib::query::to_expr(users.age) > sqllib::query::val(21);
```

### Logical Operators

Conditions can be combined using logical operators:

```cpp
// AND condition
auto combined = cond1 && cond2;

// OR condition
auto either = cond1 || cond2;

// NOT condition
auto negated = !cond;
```

### Special Conditions

Special SQL conditions are provided through helper functions:

```cpp
// IN condition
auto in_cond = sqllib::query::in(sqllib::query::to_expr(users.id), std::vector<std::string>{"1", "2", "3"});

// LIKE condition
auto like_cond = sqllib::query::like(sqllib::query::to_expr(users.name), "%Smith%");

// BETWEEN condition
auto between_cond = sqllib::query::between(sqllib::query::to_expr(users.age), "18", "65");

// IS NULL / IS NOT NULL conditions
auto null_cond = sqllib::query::is_null(sqllib::query::to_expr(users.bio));
auto not_null = sqllib::query::is_not_null(sqllib::query::to_expr(users.email));
```

## Functions and Expressions

The library supports SQL functions through the `FunctionExpr` template:

```cpp
// Count function
auto count_expr = sqllib::query::count(sqllib::query::to_expr(users.id));

// Other aggregate functions
auto sum_expr = sqllib::query::sum(sqllib::query::to_expr(users.age));
auto avg_expr = sqllib::query::avg(sqllib::query::to_expr(users.age));
auto min_expr = sqllib::query::min(sqllib::query::to_expr(users.age));
auto max_expr = sqllib::query::max(sqllib::query::to_expr(users.age));

// String functions
auto lower_expr = sqllib::query::lower(sqllib::query::to_expr(users.name));
auto upper_expr = sqllib::query::upper(sqllib::query::to_expr(users.name));
```

The library also supports special expressions like DISTINCT and CASE:

```cpp
// DISTINCT
auto distinct_expr = sqllib::query::distinct(sqllib::query::to_expr(users.age));

// CASE expression
auto case_expr = sqllib::query::case_()
    .when(cond1, sqllib::query::val("Value if true"))
    .when(cond2, sqllib::query::val("Value if second condition is true"))
    .else_(sqllib::query::val("Default value"))
    .build();
```

## Building Queries

The central component for building queries is the `SelectQuery` class template. It uses a builder pattern to construct SQL queries:

```cpp
template <
    typename Columns,
    typename Tables = std::tuple<>,
    typename Joins = std::tuple<>,
    typename Where = std::nullopt_t,
    typename GroupBys = std::tuple<>,
    typename OrderBys = std::tuple<>,
    typename HavingCond = std::nullopt_t,
    typename LimitVal = std::nullopt_t,
    typename OffsetVal = std::nullopt_t
>
class SelectQuery {
    // Implementation...
};
```

A basic query is constructed using the `select` function:

```cpp
users u;
auto query = sqllib::query::select(u.id, u.name, u.email)
    .from(u);
```

Each method call in the chain returns a new query object with the updated configuration.

### FROM Clause

The `from` method adds tables to query from:

```cpp
auto query = select(...)
    .from(users_table);

// Multiple tables
auto query = select(...)
    .from(users_table, posts_table);
```

### WHERE Clause

The `where` method adds conditions:

```cpp
auto query = select(...)
    .from(users_table)
    .where(sqllib::query::to_expr(users_table.id) == sqllib::query::val(1));
```

### JOINs

Different types of joins can be added with the join methods:

```cpp
users u;
posts p;

// Inner join
auto query = select(...)
    .from(u)
    .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));

// Left join
auto query = select(...)
    .from(u)
    .left_join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));

// Other joins: right_join, full_join, cross_join
```

### GROUP BY and HAVING

Grouping is supported with `group_by` and `having`:

```cpp
auto query = select(...)
    .from(users_table)
    .group_by(sqllib::query::to_expr(users_table.age))
    .having(sqllib::query::count(sqllib::query::to_expr(users_table.id)) > sqllib::query::val(5));
```

### ORDER BY

Results can be ordered with `order_by`:

```cpp
auto query = select(...)
    .from(users_table)
    .order_by(
        sqllib::query::desc(sqllib::query::to_expr(users_table.age)),  // Descending
        sqllib::query::asc(sqllib::query::to_expr(users_table.name))   // Ascending
    );
```

### LIMIT and OFFSET

Pagination is supported with `limit` and `offset`:

```cpp
auto query = select(...)
    .from(users_table)
    .limit(10)     // Limit to 10 results
    .offset(20);   // Skip the first 20 results
```

## Advanced Features

### Subqueries

Queries can be nested as subqueries:

```cpp
auto subquery = sqllib::query::select(...)
    .from(users_table)
    .where(...);

auto main_query = sqllib::query::select(...)
    .from(sqllib::query::as(subquery, "sub"))
    .where(...);
```

### Complex Conditions

Complex conditions can be built by combining multiple conditions:

```cpp
auto complex_condition = 
    (cond1 && cond2) || (cond3 && (cond4 || cond5));
```

## Putting It All Together

The power of the query system lies in its composability. All the components work together to build SQL queries that are:

1. **Type-safe**: The C++ compiler checks that column types match expected types
2. **SQL-injection safe**: All user-provided values are properly parameterized
3. **Expressive**: The fluent interface makes queries readable and maintainable
4. **Compile-time validated**: Errors in query construction are caught at compile time

### Complete Example

Here's a complete example showing how various components work together:

```cpp
// Define tables
users u;
posts p;
comments c;

// Build a complex query
auto query = sqllib::query::select_expr(
    sqllib::query::to_expr(u.name),
    sqllib::query::to_expr(p.title),
    sqllib::query::count(sqllib::query::to_expr(c.id)),
    sqllib::query::as(sqllib::query::sum(sqllib::query::to_expr(p.views)), "total_views")
)
.from(u)
.join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
.left_join(c, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(c.post_id)))
.where(
    (sqllib::query::to_expr(u.is_active) == sqllib::query::val(true)) &&
    (sqllib::query::to_expr(p.is_published) == sqllib::query::val(true))
)
.group_by(
    sqllib::query::to_expr(u.id),
    sqllib::query::to_expr(p.id)
)
.having(sqllib::query::count(sqllib::query::to_expr(c.id)) > sqllib::query::val(5))
.order_by(
    sqllib::query::desc(sqllib::query::to_expr(p.views))
)
.limit(10);

// Generate SQL and parameters
std::string sql = query.to_sql();
std::vector<std::string> params = query.bind_params();
```

### Execution Flow

When building and executing a query:

1. Tables and columns are defined as C++ objects
2. Query components (expressions, conditions, etc.) are composed to build a query
3. The `to_sql()` method generates the SQL string with placeholders for parameters
4. The `bind_params()` method provides the parameters to bind to the placeholders
5. The SQL and parameters are passed to a database connection for execution

This design separates the concerns of query building from query execution, making the system flexible and extensible.

## Conclusion

The SQLlib query system provides a powerful, type-safe way to build SQL queries in C++. By leveraging modern C++ features like concepts and templates, it offers compile-time safety while maintaining an expressive, fluent interface for query construction.

## Type System Deep Dive

This section provides a detailed exploration of the type system in SQLlib, showing how types are built up, composed, and evaluated. We'll examine how the library handles various edge cases and how the type system ensures safety and correctness.

### Expression Type Hierarchy

The SQLlib type system is based on a hierarchy of expression types. Here's how they relate:

```
SqlExpression (Base interface)
  ├── ColumnExpression (Column-specific expressions)
  │     ├── ColumnRef<Column> (Reference to a table column)
  │     ├── AliasedColumn<Expr> (Expression with an alias)
  │     └── FunctionExpr<Expr> (Function applied to an expression)
  │          ├── CountExpr<Expr>
  │          ├── SumExpr<Expr>
  │          └── ... (other function specializations)
  ├── Value<T> (Literal values)
  │     ├── Value<int>
  │     ├── Value<std::string>
  │     ├── Value<std::optional<T>>
  │     └── ... (other value specializations)
  ├── BinaryCondition<Left, Right> (Binary operations)
  └── ... (other specialized expressions)
```

### Type Building Examples

#### 1. Building a Simple Column Expression

When you reference a column in a query, several template instantiations occur:

```cpp
users u;
auto col_expr = sqllib::query::to_expr(u.id);

// Type of col_expr is: 
// ColumnRef<sqllib::schema::column<"id", int>>
```

The compiler deduces the template parameter for `ColumnRef` based on the column's type.

#### 2. Building a Value Expression

Value expressions wrap C++ literals with the appropriate SQL representation:

```cpp
// Integer value
auto int_val = sqllib::query::val(42);
// Type: Value<int>

// String value
auto str_val = sqllib::query::val("hello");
// Type: Value<std::string_view>

// Optional value
std::optional<int> opt = 10;
auto opt_val = sqllib::query::val(opt);
// Type: Value<std::optional<int>>
```

#### 3. Building a Condition

When building conditions, the compiler deduces complex template types:

```cpp
users u;
auto condition = sqllib::query::to_expr(u.id) == sqllib::query::val(1);

// Type of condition is:
// BinaryCondition<
//   ColumnRef<sqllib::schema::column<"id", int>>,
//   Value<int>
// >
```

#### 4. Building a Complex Query

For a SELECT query, the template types become increasingly complex:

```cpp
users u;
posts p;

auto query = sqllib::query::select(u.id, u.name)
    .from(u)
    .where(sqllib::query::to_expr(u.id) > sqllib::query::val(10))
    .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));

// Simplified type of query:
// SelectQuery<
//   std::tuple<
//     ColumnRef<sqllib::schema::column<"id", int>>,
//     ColumnRef<sqllib::schema::column<"name", std::string>>
//   >,
//   std::tuple<users>,
//   std::tuple<
//     JoinSpec<
//       posts,
//       BinaryCondition<
//         ColumnRef<sqllib::schema::column<"id", int>>,
//         ColumnRef<sqllib::schema::column<"user_id", int>>
//       >,
//       JoinType::Inner
//     >
//   >,
//   BinaryCondition<
//     ColumnRef<sqllib::schema::column<"id", int>>,
//     Value<int>
//   >
// >
```

### Type Evaluation Process

When building an SQL query, the type evaluation happens in several steps:

1. **Expression Composition**: C++ templates create composite types representing the query structure
2. **Method Chaining**: Each method in the chain returns a new query type with updated template parameters
3. **SQL Generation**: The `to_sql()` method traverses the expression tree to produce SQL
4. **Parameter Binding**: The `bind_params()` method collects parameters from the expression tree

### Edge Cases and Their Handling

#### 1. NULL Values

```cpp
// Handling NULL values with std::optional
std::optional<std::string> maybe_name;
auto cond = sqllib::query::to_expr(users.name) == sqllib::query::val(maybe_name);

// When maybe_name has no value, this generates:
// "name IS NULL" (not "name = NULL")

// When maybe_name has a value, this generates:
// "name = ?" with the value as a parameter
```

#### 2. Type Conversions

```cpp
// Implicit conversions between numeric types
auto cond = sqllib::query::to_expr(users.id) == sqllib::query::val(42.5);
// The double 42.5 is handled appropriately for comparing with an integer column

// String conversions
auto str_cond = sqllib::query::to_expr(users.id) == sqllib::query::val("42");
// The string "42" is bound as a parameter but won't force any type conversion in SQL
// This allows the database to determine how to interpret the string value
```

#### 3. Complex Nested Expressions

Complex expressions like subqueries can have deeply nested types:

```cpp
users u;
posts p;

// Subquery
auto subquery = sqllib::query::select(p.id)
    .from(p)
    .where(sqllib::query::to_expr(p.views) > sqllib::query::val(100));

// Main query using subquery
auto query = sqllib::query::select(u.id, u.name)
    .from(u)
    .where(sqllib::query::in(
        sqllib::query::to_expr(u.id),
        subquery
    ));

// The type of the query becomes very complex, with the entire subquery type
// embedded within the IN condition's template parameters
```

#### 4. Optional Query Components

SQLlib handles optional query components using `std::nullopt_t` in template parameters:

```cpp
// Query with no WHERE clause
auto query1 = sqllib::query::select(u.id, u.name)
    .from(u);
// The Where type parameter is std::nullopt_t

// Query with WHERE clause
auto query2 = query1.where(sqllib::query::to_expr(u.id) > sqllib::query::val(10));
// Now the Where parameter is the condition's type
```

#### 5. Function Expression Types

Function calls create specialized expression types:

```cpp
// COUNT function
auto count_expr = sqllib::query::count(sqllib::query::to_expr(u.id));
// Type: CountExpr<ColumnRef<sqllib::schema::column<"id", int>>>

// Nested function calls
auto nested_func = sqllib::query::upper(
    sqllib::query::concat(
        sqllib::query::to_expr(u.first_name),
        sqllib::query::val(" "),
        sqllib::query::to_expr(u.last_name)
    )
);
// This creates a deeply nested function expression type
```

### CASE Expressions

CASE expressions use a builder pattern which creates a complex nested type structure:

```cpp
auto case_expr = sqllib::query::case_()
    .when(
        sqllib::query::to_expr(u.age) < sqllib::query::val(18),
        sqllib::query::val("Minor")
    )
    .when(
        sqllib::query::to_expr(u.age) < sqllib::query::val(65),
        sqllib::query::val("Adult")
    )
    .else_(
        sqllib::query::val("Senior")
    )
    .build();

// The resulting type contains all the when conditions and result expressions
// in a nested tuple structure
```

### Table Join Types

Join operations create complex nested types representing the structure:

```cpp
users u;
posts p;
comments c;

auto query = sqllib::query::select(u.id, p.title)
    .from(u)
    .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
    .left_join(c, sqllib::query::on(sqllib::query::to_expr(p.id) == sqllib::query::to_expr(c.post_id)));

// The Joins template parameter becomes a tuple of JoinSpec objects, each with:
// - The table type
// - The join condition type (a binary condition)
// - The join type (Inner, Left, Right, etc.)
```

### Compile-Time Type Safety

One of the key advantages of SQLlib's type system is compile-time error detection:

```cpp
users u;

// This will not compile: cannot compare int column with boolean value
auto error_condition = sqllib::query::to_expr(u.id) == sqllib::query::val(true);

// This will not compile: unknown column
auto error_column = sqllib::query::to_expr(u.unknown_column);

// This will not compile: condition required in WHERE clause
auto error_query = sqllib::query::select(u.id).from(u).where(u.name);
```

### Template Type Deduction Guidelines

When working with SQLlib, understanding template type deduction can help prevent issues:

1. **Always wrap columns**: Use `sqllib::query::to_expr(column)` to ensure proper type deduction
2. **Use value wrappers**: Always use `sqllib::query::val()` for literal values
3. **Explicit function templates**: For complex expressions, explicitly specify template parameters if needed

### Type Manipulation

Advanced users can manipulate and extend the type system:

```cpp
// Define a custom SQL function
template <sqllib::query::SqlExpr Expr>
class MyCustomFunction : public sqllib::query::ColumnExpression {
public:
    explicit MyCustomFunction(Expr expr) : expr_(std::move(expr)) {}
    
    std::string to_sql() const override {
        return "MY_FUNC(" + expr_.to_sql() + ")";
    }
    
    std::vector<std::string> bind_params() const override {
        return expr_.bind_params();
    }
    
    std::string column_name() const override {
        return "MY_FUNC(" + expr_.column_name() + ")";
    }
    
    std::string table_name() const override {
        return "";
    }
    
private:
    Expr expr_;
};

// Helper function to create the custom function
template <sqllib::query::SqlExpr Expr>
auto my_func(Expr expr) {
    return MyCustomFunction<Expr>(std::move(expr));
}

// Usage
auto func_expr = my_func(sqllib::query::to_expr(u.name));
```

### Performance Implications

The complex template types in SQLlib have minimal runtime impact because:

1. Most type complexity is resolved at compile time
2. The resulting executable code is highly optimized
3. Modern compilers can effectively inline and optimize the expression trees

However, compilation times might increase with very complex queries due to template instantiation depth.

### Future-Proofing Types

SQLlib's type system is designed to be extensible for future SQL features:

1. **Concepts-based design**: New types need only satisfy the relevant concepts
2. **Composition over inheritance**: New features can be added through composition
3. **Expression tree pattern**: Can represent virtually any SQL expression structure

This approach allows the library to evolve while maintaining backward compatibility. 