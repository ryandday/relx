## Schema Enhancements

SQLlib now includes several schema declaration enhancements that provide more control over your database tables:

### 1. Nullable Types with std::optional Integration

Columns can now be defined as nullable using the C++17 `std::optional` type:

```cpp
// Define a nullable column
column<"email", std::optional<std::string>> email;

// The column will be defined without NOT NULL constraint in SQL
// SQL: email TEXT

// For backward compatibility, the older syntax still works
nullable_column<"phone", std::string> phone;
```

### 2. Check Constraints

Check constraints allow you to define business rules directly in your schema:

```cpp
// Column-level check constraint
check_constraint<&Product::price, "price > 0"> positive_price;

// Table-level check constraint
check_constraint<nullptr, "stock >= 0 AND (status = 'active' OR status = 'discontinued')"> valid_product;
```

When creating tables, these constraints will be added to the SQL definition:
```sql
CREATE TABLE products (
  -- other columns
  price REAL NOT NULL,
  stock INTEGER NOT NULL,
  status TEXT NOT NULL,
  -- other columns
  CHECK (price > 0),
  CHECK (stock >= 0 AND (status = 'active' OR status = 'discontinued'))
);
```

### 3. Default Values

Columns can now have default values specified at compile time:

```cpp
// Simple default values for different types
column<"stock", int, default_value<0>> stock;
column<"is_active", bool, default_value<true>> is_active;
column<"price", double, default_value<0.0>> price;

// SQL expression default values
column<"created_at", timestamp, default_value<"CURRENT_TIMESTAMP">> created_at;
column<"status", std::string, default_value<"'active'">> status;

// NULL default for nullable columns
column<"notes", std::optional<std::string>, null_default> notes;
```

### 4. Unique Constraints

Define unique constraints at both column and table levels:

```cpp
// Single column unique constraint
unique_constraint<&User::email> unique_email;

// Multi-column unique constraint
composite_unique_constraint<&User::first_name, &User::last_name> unique_name;

// Alternative syntax for multi-column unique constraint
unique<&User::department, &User::position> unique_dept_pos;
```

These constraints will be added to your CREATE TABLE statement:

```sql
CREATE TABLE users (
  -- columns
  UNIQUE (email),
  UNIQUE (first_name, last_name),
  UNIQUE (department, position)
);
```

### Complete Example

Here's a complete example using all these features:

```cpp
struct Product {
    static constexpr auto name = std::string_view("products");
    
    column<"id", int> id;
    column<"product_name", std::string> product_name;
    column<"price", double, default_value<0.0>> price;
    column<"description", std::optional<std::string>> description;
    column<"stock", int, default_value<10>> stock;
    column<"active", bool, default_value<true>> active;
    
    primary_key<&Product::id> pk;
    unique_constraint<&Product::product_name> unique_name;
    check_constraint<&Product::price, "price >= 0"> valid_price;
    check_constraint<nullptr, "stock >= 0"> valid_stock;
};
``` 