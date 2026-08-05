# Schema Definition in relx

A table is a plain C++ aggregate carrying relx annotations. Column names come from the member
identifiers, types from the member types, and constraints from annotations — so the struct is the
single source of truth, and it doubles as the DTO for query results.

Everything here requires C++26 reflection (P2996 + P3394): GCC 16.1 with `-freflection`. See the
[Development Guide](development.md).

## Table of Contents
- [Basic Table Definition](#basic-table-definition)
- [Column Types](#column-types)
- [Column Annotations](#column-annotations)
- [Foreign Keys](#foreign-keys)
- [Table-Level Annotations](#table-level-annotations)
- [Enums](#enums)
- [Generating DDL](#generating-ddl)
- [Compile-Time Validation](#compile-time-validation)

## Basic Table Definition

```cpp
#include <relx/schema.hpp>
#include <optional>
#include <string>

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string username;
  [[=relx::ann::unique]] std::string email;
  [[=relx::default_value<true>{}]] bool active;
  [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
  std::optional<std::string> bio;
};

inline constexpr auto users = relx::t<Users>;
```

`relx::t<Users>` is the table object queries are written against — reflection synthesizes one column
member per field, with the same names, so `users.email` is a column reference. Define it once next
to the struct.

```cpp
auto q = relx::select(users.id, users.username).from(users).where(users.id == 42);
auto rows = conn.execute_many<Users>(q);
```

`relx::c<^^Users::id>` is a standalone column reference for contexts where no table object is in
scope; it is otherwise identical to `users.id`.

The `[[=relx::table("...")]]` annotation is optional. Without it the table name is the struct
identifier verbatim — `struct AuditLog { ... }` becomes `CREATE TABLE AuditLog`, which PostgreSQL
folds to lowercase. Annotate when you want a name that differs from the identifier.

## Column Types

| C++ Type | SQL Type |
|----------|----------|
| `int` | `INTEGER` |
| `long` / `long long` | `BIGINT` |
| `float` | `REAL` |
| `double` | `DOUBLE PRECISION` |
| `bool` | `BOOLEAN` |
| `std::string` | `TEXT` |
| enum type | `TEXT` + a `CHECK` restricting it to the enumerators (see [Enums](#enums)) |
| `std::chrono::system_clock::time_point` | `TIMESTAMPTZ` |
| `std::chrono::year_month_day` | `DATE` |
| `boost::uuids::uuid` | `UUID` |
| `std::optional<T>` | SQL type of `T`, nullable |

A non-optional member is `NOT NULL`; wrapping it in `std::optional` is the only way to make a column
nullable.

Add your own type by specializing `relx::schema::column_traits<T>` with `sql_type_name`,
`nullable`, `to_sql_string`, and `from_sql_string` — see `include/relx/schema/uuid_traits.hpp` for a
short example.

## Column Annotations

| Annotation | Emits |
|------------|-------|
| `[[=relx::ann::pk]]` | ` PRIMARY KEY` |
| `[[=relx::ann::unique]]` | ` UNIQUE` |
| `[[=relx::ann::autoincrement]]` | ` GENERATED ALWAYS AS IDENTITY` |
| `[[=relx::identity<Start, Increment, Min, Max, Cycle>{}]]` | identity with explicit options |
| `[[=relx::default_value<V>{}]]` | ` DEFAULT V` (integral, bool, floating-point, or enum) |
| `[[=relx::string_default<"s">{}]]` | ` DEFAULT 's'` (quoted) |
| `[[=relx::default_sql<"now()">{}]]` | ` DEFAULT now()` (unquoted SQL expression) |
| `[[=relx::null_default{}]]` | ` DEFAULT NULL` |
| `[[=relx::schema::check<"expr">{}]]` | ` CHECK(expr)` |
| `[[=relx::ann::native_enum]]` | stores an enum as a native database enum type |
| `[[=relx::ann::fk<^^Other::col>]]` | ` REFERENCES other(col)` |
| `[[=relx::on_delete<"CASCADE">{}]]` / `[[=relx::on_update<"SET NULL">{}]]` | FK actions |

Several annotations can share one attribute list, and they apply in the order written:

```cpp
struct [[=relx::table("posts")]] Posts {
  [[=relx::ann::pk, =relx::ann::autoincrement]] int id;
  [[=relx::ann::fk<^^Users::id>, =relx::on_delete<"CASCADE">{}]] int user_id;
  std::string title;
  [[=relx::schema::check<"views >= 0">{}]] int views;
  [[=relx::null_default{}]] std::optional<std::string> summary;
};
inline constexpr auto posts = relx::t<Posts>;
```

```sql
CREATE TABLE posts (
id INTEGER NOT NULL PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
title TEXT NOT NULL,
views INTEGER NOT NULL CHECK(views >= 0),
summary TEXT DEFAULT NULL
);
```

`default_sql<"...">` is a spelling of `string_default<"...", /*IsLiteral=*/true>`; both leave the
value unquoted, which is what SQL functions and keywords like `CURRENT_TIMESTAMP` need.
Writing a known SQL expression (`CURRENT_TIMESTAMP`, `now()`, …) in a plain `string_default`
is a compile error pointing at `default_sql` — the quoted string is never what you meant.

`relx::identity<>` takes start, increment, minimum, maximum, and a cycle flag, emitting only the
options that differ from the defaults — `[[=relx::identity<100, 5>{}]]` gives
` GENERATED ALWAYS AS IDENTITY (START WITH 100 INCREMENT BY 5)`.

## Foreign Keys

`ann::fk` names the referenced column by reflection, so a typo or a renamed target column is a
compile error rather than a runtime DDL failure:

```cpp
[[=relx::ann::fk<^^Users::id>]] int user_id;
```

The referenced struct must be **complete** at the point of annotation — define referenced tables
before the tables that point at them.

### Referential Actions

`on_delete` and `on_update` are separate annotations on the same field, and the SQL clauses come out
in the order the annotations are written:

```cpp
[[=relx::ann::fk<^^Users::id>, =relx::on_delete<"CASCADE">{}]] int user_id;
// user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE
```

There is no `no_action` marker — omit the annotation and no clause is emitted, which is exactly
PostgreSQL's `NO ACTION` default. Writing `on_update<"NO ACTION">{}` is legal but emits the clause
literally.

### Self-Referencing Foreign Keys

A struct may reference its own members, but only those already declared above the annotation —
`^^Categories::id` resolves against the partially-declared class:

```cpp
struct [[=relx::table("categories")]] Categories {
  [[=relx::ann::pk]] int id;
  std::string name;
  [[=relx::ann::fk<^^Categories::id>, =relx::on_delete<"SET NULL">{}]] std::optional<int> parent_id;
};
```

If the referencing column has to come *before* the column it points at, `^^Categories::id` fails to
compile with `'Categories::id' has not been declared`. Name the target as strings instead, using the
raw modifier the annotation would have produced:

```cpp
struct [[=relx::table("categories")]] Categories {
  [[=relx::schema::references<"categories", "id">{}, =relx::on_delete<"SET NULL">{}]]
  std::optional<int> parent_id;
  [[=relx::ann::pk]] int id;
};
```

Both spellings emit `parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL`. The string form
gives up the compile-time check on the target column, so prefer `ann::fk` and order the members to
suit it where you can.

## Table-Level Annotations

Constraints spanning more than one column go on the struct. An annotation cannot reflect on the
struct's own members while the struct is still incomplete, so local columns are named as strings;
every name is checked against the members when DDL is generated.

```cpp
struct [[=relx::table("order_items"),
        =relx::ann::composite_pk("order_id", "product_id"),
        =relx::ann::composite_unique("region", "external_ref"),
        =relx::ann::index_on("customer_id", "created_at"),
        =relx::ann::index_on("external_ref").unique(),
        =relx::ann::check("quantity > 0").named("positive_quantity")]] OrderItems {
  int order_id;
  int product_id;
  int customer_id;
  std::string region;
  std::string external_ref;
  std::string created_at;
  int quantity;
};
```

```sql
CREATE TABLE order_items (
order_id INTEGER NOT NULL,
product_id INTEGER NOT NULL,
customer_id INTEGER NOT NULL,
region TEXT NOT NULL,
external_ref TEXT NOT NULL,
created_at TEXT NOT NULL,
quantity INTEGER NOT NULL,
PRIMARY KEY (order_id, product_id),
UNIQUE (region, external_ref),
CONSTRAINT positive_quantity CHECK (quantity > 0)
);
```

Table-level clauses appear after the columns in annotation order. Indexes are **not** part of
`CREATE TABLE` — `index_on` produces separate statements, see [Generating DDL](#generating-ddl).

`ann::check` is the table-level `CHECK (expr)`; the column modifier `schema::check<"expr">` renders
` CHECK(expr)` inline on its column. `.named("...")` prefixes `CONSTRAINT <name> `.

A composite foreign key names its local columns as strings and its targets by reflection:

```cpp
struct [[=relx::table("shipments"),
        =relx::ann::composite_fk<^^OrderItems::order_id, ^^OrderItems::product_id>(
            "order_id", "product_id")]] Shipments {
  [[=relx::ann::pk]] int id;
  int order_id;
  int product_id;
};
```

```sql
FOREIGN KEY (order_id, product_id) REFERENCES order_items (order_id, product_id)
```

The targets must all belong to one table and must form that table's primary key or a declared unique
set — the same rule PostgreSQL enforces at `CREATE TABLE` time, checked here at compile time.

## Enums

An enum column defaults to `TEXT` holding the enumerator identifier, plus a generated `CHECK`:

```cpp
enum class Status { active, inactive, banned };

struct [[=relx::table("accounts")]] Accounts {
  [[=relx::ann::pk]] int id;
  Status status;
  [[=relx::ann::native_enum]] Status native_status;
  [[=relx::default_value<Status::active>{}]] Status defaulted;
};
```

```sql
CREATE TABLE accounts (
id INTEGER NOT NULL PRIMARY KEY,
status TEXT NOT NULL CHECK(status IN ('active', 'inactive', 'banned')),
native_status status NOT NULL,
defaulted TEXT NOT NULL DEFAULT 'active' CHECK(defaulted IN ('active', 'inactive', 'banned'))
);
```

`ann::native_enum` switches the column to a real PostgreSQL enum type, named after the enum
identifier lowercased. That type has to exist before the table:

```cpp
relx::value_or_throw(conn.execute_raw(std::string(relx::create_enum_type_sql<Status>())));
// CREATE TYPE status AS ENUM ('active', 'inactive', 'banned');
```

`relx::drop_enum_type_sql<Status>()` produces `DROP TYPE IF EXISTS status;`.

## Generating DDL

DDL is built at compile time. `to_sql()` is `consteval` and returns a `std::string_view` into static
storage, so the statement costs nothing at runtime:

```cpp
constexpr auto ddl = relx::create_table_sql<Users>().if_not_exists().to_sql();
```

```sql
CREATE TABLE IF NOT EXISTS users (
id INTEGER NOT NULL PRIMARY KEY,
username TEXT NOT NULL,
email TEXT NOT NULL UNIQUE,
active BOOLEAN NOT NULL DEFAULT true,
created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
bio TEXT
);
```

| Call | Produces |
|------|----------|
| `relx::create_table_sql<T>()` | `CREATE TABLE`, with `.if_not_exists()` |
| `relx::drop_table_sql<T>()` | `DROP TABLE`, with `.if_exists()` and `.cascade()` |
| `relx::create_indexes_sql<T>()` | `std::array` of `CREATE INDEX` statements, one per `index_on` |
| `relx::create_enum_type_sql<E>()` / `relx::drop_enum_type_sql<E>()` | `CREATE`/`DROP TYPE` |

```cpp
for (std::string_view stmt : relx::create_indexes_sql<OrderItems>()) {
  relx::value_or_throw(conn.execute_raw(std::string(stmt)));
}
// CREATE INDEX order_items_customer_id_created_at_idx ON order_items (customer_id, created_at)
// CREATE UNIQUE INDEX order_items_external_ref_idx ON order_items (external_ref)
```

Index names are generated as `<table>_<columns joined by _>_idx`, and the statements carry no
trailing semicolon.

The runtime builders `relx::create_table(relx::t<T>)` and `relx::drop_table(relx::t<T>)` produce the
same SQL as executable query objects:

```cpp
auto result = conn.execute(relx::create_table(users).if_not_exists());
```

Use the runtime form for tables with floating-point `DEFAULT` values — constexpr floating-point
formatting is not available, so those cannot go through the `consteval` builders.

## Compile-Time Validation

The following are compile errors, not runtime surprises:

- naming a column that does not exist in a struct-level annotation
- a struct-level annotation naming no columns at all
- more than one `composite_pk`, or a `composite_pk` alongside a column-level `ann::pk`
- a `composite_fk` whose targets span tables, or do not form the target's primary key or a declared
  unique set
- `ann::fk<^^Other::col>` where `col` is not a member of `Other`

Diagnostics name the offending table and column.
