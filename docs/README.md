# relx Documentation

relx is a C++26 library for building and executing SQL queries with compile-time type safety.
Schemas are plain structs carrying reflection annotations; the same struct doubles as the DTO for
query results.

| Guide | Description |
|-------|-------------|
| [Schema Definition](schema-definition.md) | Tables, columns, constraints, and DDL generation |
| [Query Building](query-building.md) | SELECT, INSERT, UPDATE, DELETE with the fluent API |
| [Result Parsing](result-parsing.md) | Mapping query results onto C++ types |
| [Streaming Results](streaming-results.md) | Large result sets with constant memory usage |
| [Migrations](migrations.md) | Schema diffing and DDL migration generation |
| [Error Handling](error-handling.md) | Working with `expected`-based results |
| [Performance Guide](performance.md) | Optimization tips and memory trade-offs |
| [Development Guide](development.md) | Toolchain, container workflow, building and testing |
| [C++26 Reflection Plan](cpp26-reflection-plan.md) | Why reflection, and what it replaced |

Requires **GCC 16.1+** with `-freflection` — the only released compiler implementing C++26
reflection (P2996). See the [Development Guide](development.md).

## Getting Started

```cpp
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>
#include <print>

// clang-format off
struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string name;
  [[=relx::ann::unique]] std::string email;
};
// clang-format on

inline constexpr auto users = relx::t<Users>;

int main() {
  relx::PostgreSQLConnection conn({.host = "localhost",
                                   .port = 5432,
                                   .dbname = "example",
                                   .user = "postgres",
                                   .password = "postgres",
                                   .application_name = "relx_example"});

  auto connect_result = conn.connect();
  if (!connect_result) {
    std::println("Connection error: {}", connect_result.error().message);
    return 1;
  }

  // value_or_throw unwraps an expected that carries a value; throw_if_failed is its
  // counterpart for expected<void> results such as connect()
  relx::value_or_throw(conn.execute(relx::create_table(users).if_not_exists()));

  relx::value_or_throw(conn.execute(relx::insert_into(users)
                                        .columns(users.name, users.email)
                                        .values("John Doe", "john@example.com")));

  auto query = relx::select(users.id, users.name, users.email)
                   .from(users)
                   .where(users.name.like("%John%"));

  // Results map onto Users itself; a separate DTO struct works the same way
  auto result = conn.execute_many<Users>(query);
  if (result) {
    for (const auto& user : *result) {
      std::println("User: {} - {} ({})", user.id, user.name, user.email);
    }
  }
}
```

## Architecture

- **Core library** (header-only): schema definitions, query building, SQL generation
- **PostgreSQL client**: synchronous database operations over libpq
- **PostgreSQL async client**: coroutine-based operations with Boost.Asio
- **Connection pool**: managed pooling for high-concurrency applications

## Reading Order

1. [Schema Definition](schema-definition.md) — define your database structure as C++ types
2. [Query Building](query-building.md) — the fluent query API
3. [Result Parsing](result-parsing.md) — getting results back into C++ types
4. [Streaming Results](streaming-results.md) — processing large datasets lazily
5. [Migrations](migrations.md) — evolving a schema over time
6. [Error Handling](error-handling.md) — robust error management
