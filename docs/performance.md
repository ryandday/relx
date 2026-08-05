# Performance Guide

This guide covers performance best practices when using relx for building and executing SQL queries.

## Table of Contents

- [Runtime Performance](#runtime-performance)
- [Connection Management](#connection-management)
- [Query Optimization](#query-optimization)
- [Memory Management](#memory-management)
- [Streaming for Large Datasets](#streaming-for-large-datasets)
- [Benchmarking](#benchmarking)

## Runtime Performance

### Connection Pooling

Use connection pools for high-throughput applications:

```cpp
// Configure connection pool for optimal performance
relx::PostgreSQLConnectionPoolConfig config{
    .connection_params = {/* connection details */},
    .initial_size = 10,     // Start with 10 connections
    .max_size = 50,         // Allow up to 50 connections
    .connection_timeout = std::chrono::milliseconds(5000),
    .validate_connections = true,
    .max_idle_time = std::chrono::minutes(5)
};

auto pool = relx::PostgreSQLConnectionPool::create(config);
```

### Batch Operations

Use batch inserts for better performance:

```cpp
// Efficient: one statement, several rows
auto batch_insert = relx::insert_into(users)
    .columns(users.name, users.email)
    .values("John Doe", "john@example.com")
    .values("Jane Smith", "jane@example.com");

auto result = conn.execute(batch_insert);
```

Every `.values(...)` produces a **new query type**, so rows cannot be accumulated in a runtime loop —
the row count is part of the type. For a row count only known at runtime, either fold over the list
at compile time, or build the statement from a `std::vector` of parameters and send it through
`conn.execute_raw`.

## Connection Management

### Connection Lifecycle

Minimize connection overhead by reusing connections:

```cpp
// Good: Reuse connection for multiple operations
relx::PostgreSQLConnection conn(params);
auto conn_result = conn.connect();

// Multiple operations on same connection
auto result1 = conn.execute(query1);
auto result2 = conn.execute(query2);
auto result3 = conn.execute(query3);

// Connection automatically closed on destruction
```

### Async Operations for I/O Bound Work

Use async connections for I/O intensive applications:

```cpp
boost::asio::awaitable<void> process_users() {
    relx::PostgreSQLAsyncConnection conn(io_context, params);
    
    auto connect_result = co_await conn.connect();
    if (!connect_result) {
        co_return;
    }
    
    auto result = co_await conn.execute_many<UserDTO>(query);
    
    co_await conn.disconnect();
}
```

## Query Optimization

### Index Usage

Ensure your queries use appropriate database indexes:

```cpp
// clang-format off
struct [[=relx::table("users"),
        =relx::ann::index_on("email"),
        =relx::ann::index_on("status")]] Users {
    [[=relx::ann::pk]] int id;
    std::string email;
    std::string status;
};
// clang-format on
inline constexpr auto users = relx::t<Users>;
```

Indexes are not part of `CREATE TABLE`; run them separately:

```cpp
for (std::string_view stmt : relx::create_indexes_sql<Users>()) {
    relx::value_or_throw(conn.execute_raw(std::string(stmt)));
}
```

### Selective Column Queries

Only select columns you need:

```cpp
// Good: Select only required columns
auto query = relx::select(users.id, users.name)
    .from(users);

// Avoid: Selecting all columns when not needed
auto query = relx::select(users.id, users.name, users.email, 
                         users.created_at, users.updated_at)
    .from(users);
```

### Efficient WHERE Clauses

Structure WHERE clauses for optimal index usage:

```cpp
Users users;

// Good: Uses index on status column first
auto query = relx::select(users.id, users.name)
    .from(users)
    .where(users.status == "active" && users.created_at > "2023-01-01");

// Consider index order and selectivity
```

## Memory Management

### Result Set Processing

Process large result sets efficiently:

```cpp
auto result = conn.execute_many<UserDTO>(query);
if (result) {
    for (const auto& user : *result) {
        process_user(user);
    }
}
```

`execute_many<T>` still materializes every row; for result sets that should never all be resident,
see [Streaming for Large Datasets](#streaming-for-large-datasets) below.

### RAII for Resource Management

Leverage RAII for automatic resource cleanup:

```cpp
void process_data() {
    relx::PostgreSQLConnection conn(params);
    auto conn_result = conn.connect();
    
    // Resources automatically cleaned up on scope exit
    auto result = conn.execute_many<UserDTO>(query);
    
    // No manual cleanup needed
} // Connection automatically closed here
```

## Streaming for Large Datasets

### When to Use Streaming

Use streaming for optimal performance in these scenarios:

- **Large Result Sets**: >10,000 rows or >100MB of data
- **Memory Constraints**: Limited RAM environment
- **Real-time Processing**: Need to start processing immediately
- **ETL Operations**: Data transformation pipelines

### Memory Usage Comparison

```cpp
// Traditional approach - the whole result set lands in memory
auto regular_result = conn.execute_many<UserDTO>(
    relx::select_all(users).limit(1000000)
);

// Streaming approach - one row in memory at a time
#include <relx/connection/postgresql_streaming_source.hpp>

auto streaming_query = relx::select_all(users).limit(1000000);
auto streaming_result = relx::connection::create_streaming_result(
    conn, streaming_query.to_sql(), 1000000);

for (const auto& lazy_row : streaming_result) {
    auto id = lazy_row.get<int>("id");
    auto name = lazy_row.get<std::string>("name");

    if (id && name) {
        process_user(*id, *name);
    }
}
```

### Lazy Parsing Performance

Lazy parsing improves performance by deferring type conversion:

```cpp
// Traditional: Parse all columns immediately
auto result = conn.execute_many<UserDTO>(query);
for (const auto& user : *result) {
    // All fields already parsed, even if not used
    if (user.is_premium) {
        process_premium_features(user.preferences);
    }
}

// Lazy: Parse only what you need
auto lazy_result = relx::result::parse_lazy(query, std::move(raw_results));
for (const auto& lazy_row : lazy_result) {
    auto is_premium = lazy_row.get<bool>("is_premium");
    
    if (is_premium && *is_premium) {
        // Only parse expensive fields when needed
        auto preferences = lazy_row.get<std::string>("preferences");
        if (preferences) {
            process_premium_features(*preferences);
        }
    }
}
```

### Async Streaming for Concurrency

Use async streaming for maximum throughput:

```cpp
#include <relx/connection/postgresql_async_streaming_source.hpp>

boost::asio::awaitable<void> high_performance_processing() {
    relx::connection::PostgreSQLAsyncConnection conn(io_context, params);
    co_await conn.connect();
    
    auto async_query = relx::select_all(large_table).order_by(large_table.id);
    auto streaming_result = relx::connection::create_async_streaming_result(
        conn, async_query.to_sql());
    
    int batch_count = 0;
    const int BATCH_SIZE = 1000;
    
    co_await streaming_result.for_each([&](const auto& lazy_row) -> boost::asio::awaitable<void> {
        // Process row asynchronously
        co_await process_row_async(lazy_row);
        
        batch_count++;
        if (batch_count % BATCH_SIZE == 0) {
            // Yield control periodically for better concurrency
            co_await boost::asio::this_coro::executor;
        }
        
        co_return;
    });
}
```

### Choosing an Approach

| Approach | Memory | Best for |
|----------|--------|----------|
| Regular query | proportional to result size | small results, random access, multiple passes |
| Synchronous streaming | constant | large results processed once, in order |
| Async streaming | constant | large results, with other I/O in flight |
| Lazy parsing | proportional to raw result | wide rows where most columns go unread |

Lazy parsing saves *conversion* work, not memory — the raw result is still fully in memory.

### Streaming Best Practices

```cpp
// clang-format off
struct [[=relx::table("recent_events")]] RecentEvents {
    [[=relx::ann::pk]] int id;
    std::string timestamp;
};

struct [[=relx::table("huge_table")]] HugeTable {
    [[=relx::ann::pk]] int id;
};
// clang-format on
inline constexpr auto recent_events = relx::t<RecentEvents>;
inline constexpr auto huge_table = relx::t<HugeTable>;

// ✅ Good: Order by indexed columns for efficient streaming
auto good_query = relx::select_all(users).order_by(users.id);  // id is primary key
auto streaming_result = relx::connection::create_streaming_result(conn, good_query.to_sql());

// ✅ Good: Use LIMIT with streaming for bounded processing
auto limited_query = relx::select_all(recent_events)
    .order_by(relx::desc(recent_events.timestamp))
    .limit(10000);
auto streaming_result2 = relx::connection::create_streaming_result(
    conn, limited_query.to_sql(), 10000);

// ❌ Avoid: Unordered streaming of very large tables
auto bad_query = relx::select_all(huge_table);  // No ORDER BY
```

### Automatic Resource Management

relx provides RAII-based cleanup for optimal performance:

```cpp
{
    auto cleanup_query = relx::select_all(users);
    auto streaming_result = relx::connection::create_async_streaming_result(
        conn, cleanup_query.to_sql());

    auto it = streaming_result.begin();
    co_await it.advance();

    // Early exit before the rows run out is fine
    if (found_target) {
        // ... use *it, then leave the scope
    }

} // Destructor automatically resets connection state

// Connection immediately ready for next operation
auto next_result = co_await conn.execute_raw("SELECT COUNT(*) FROM orders");
```

## Benchmarking

### Query Performance Measurement

Measure query execution time:

```cpp
#include <chrono>

void benchmark_query() {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto result = conn.execute_many<UserDTO>(query);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::println("Query executed in {} ms", duration.count());
}
```

### Connection Pool Metrics

Monitor connection pool performance:

```cpp
std::println("Active connections: {}", pool->active_connections());
std::println("Idle connections: {}", pool->idle_connections());
```

## Performance Tips Summary

1. **Use connection pools** for high-concurrency applications
2. **Reuse query objects** when executing similar queries multiple times
3. **Use batch operations** for bulk data operations
4. **Select only required columns** to minimize data transfer
5. **Create appropriate indexes** for frequently queried columns
6. **Use async connections** for I/O bound applications
7. **Measure and profile** your application's database performance
8. **Leverage RAII** for automatic resource management
