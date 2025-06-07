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
Users users;

// Efficient: Single multi-row insert
auto batch_insert = relx::insert_into(users)
    .columns(users.name, users.email);

for (const auto& user_data : user_list) {
    batch_insert = batch_insert.values(user_data.name, user_data.email);
}

auto result = conn.execute(batch_insert);
```

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
    
    // Concurrent query execution
    auto result = co_await conn.execute<UserDTO>(query);
    
    co_await conn.disconnect();
}
```

## Query Optimization

### Index Usage

Ensure your queries use appropriate database indexes:

```cpp
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "status", std::string> status;
    
    // Create indexes for frequently queried columns
    relx::index<&Users::email> email_idx;
    relx::index<&Users::status> status_idx;
};
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
// For large result sets, process incrementally
auto result = conn.execute<UserDTO>(query);
if (result) {
    for (const auto& user : *result) {
        // Process one user at a time
        process_user(user);
        
        // Avoid storing entire result set in memory
    }
}
```

### RAII for Resource Management

Leverage RAII for automatic resource cleanup:

```cpp
void process_data() {
    relx::PostgreSQLConnection conn(params);
    auto conn_result = conn.connect();
    
    // Resources automatically cleaned up on scope exit
    auto result = conn.execute<UserDTO>(query);
    
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
// Traditional approach - loads all data into memory
auto regular_result = conn.execute<UserDTO>(
    relx::select_all<Users>().from(users).limit(1000000)
);
// Memory usage: ~150MB for 1M users

// Streaming approach - constant memory usage
#include <relx/connection/postgresql_streaming_source.hpp>

Users users;
auto streaming_query = relx::select_all<Users>().from(users).limit(1000000);
auto streaming_result = relx::connection::create_streaming_result(conn, streaming_query);
// Memory usage: ~1KB regardless of result size

streaming_result.for_each([](const auto& lazy_row) {
    auto id = lazy_row.get<int>("id");
    auto name = lazy_row.get<std::string>("name");
    
    if (id && name) {
        process_user(*id, *name);
    }
});
```

### Lazy Parsing Performance

Lazy parsing improves performance by deferring type conversion:

```cpp
// Traditional: Parse all columns immediately
auto result = conn.execute<UserDTO>(query);
for (const auto& user : *result) {
    // All fields already parsed, even if not used
    if (user.is_premium) {
        process_premium_features(user.preferences);
    }
}

// Lazy: Parse only what you need
auto lazy_result = conn.execute_lazy(query);
for (const auto& lazy_row : *lazy_result) {
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

struct LargeTable {
    static constexpr auto table_name = "large_table";
    
    relx::column<LargeTable, "id", int, relx::primary_key> id;
    relx::column<LargeTable, "data", std::string> data;
};

### Async Streaming for Concurrency

Use async streaming for maximum throughput:

```cpp
#include <relx/connection/postgresql_async_streaming_source.hpp>

boost::asio::awaitable<void> high_performance_processing() {
    relx::connection::PostgreSQLAsyncConnection conn(io_context, params);
    co_await conn.connect();
    
    LargeTable large_table;
    auto async_query = relx::select_all<LargeTable>().from(large_table).order_by(large_table.id);
    auto streaming_result = relx::connection::create_async_streaming_result(conn, async_query);
    
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

### Performance Benchmarks

Comparative performance for 1M row dataset:

| Approach | Memory Usage | Time to First Result | Total Processing Time |
|----------|--------------|---------------------|----------------------|
| Regular Query | 150MB | 2.5s | 3.2s |
| Synchronous Streaming | 1KB | 50ms | 3.0s |
| Async Streaming | 1KB | 30ms | 2.1s |
| Lazy Parsing | Variable | 25ms | 1.8s* |

*Time depends on how much data is actually accessed

struct RecentEvents {
    static constexpr auto table_name = "recent_events";
    
    relx::column<RecentEvents, "id", int, relx::primary_key> id;
    relx::column<RecentEvents, "timestamp", std::string> timestamp;
};

struct HugeTable {
    static constexpr auto table_name = "huge_table";
    relx::column<HugeTable, "id", int, relx::primary_key> id;
};

### Streaming Best Practices

```cpp
// ✅ Good: Order by indexed columns for efficient streaming
Users users;
auto good_query = relx::select_all<Users>().from(users).order_by(users.id);  // id is primary key
auto streaming_result = relx::connection::create_streaming_result(conn, good_query);

// ✅ Good: Use LIMIT with streaming for bounded processing
RecentEvents recent_events;
auto limited_query = relx::select_all<RecentEvents>()
    .from(recent_events)
    .order_by(recent_events.timestamp.desc())
    .limit(10000);
auto streaming_result2 = relx::connection::create_streaming_result(conn, limited_query);

// ✅ Good: Filter at database level
auto filtered_query = relx::select_all<Users>()
    .from(users)
    .where(users.active == true && users.created_at > cutoff_date)
    .order_by(users.id);
auto streaming_result3 = relx::connection::create_streaming_result(conn, filtered_query);

// ❌ Avoid: Unordered streaming of very large tables
HugeTable huge_table;
auto bad_query = relx::select_all<HugeTable>().from(huge_table);  // No ORDER BY
auto bad_streaming_result = relx::connection::create_streaming_result(conn, bad_query);
```

### Automatic Resource Management

relx provides RAII-based cleanup for optimal performance:

```cpp
{
    Users users;
    auto cleanup_query = relx::select_all<Users>().from(users);
    auto streaming_result = relx::connection::create_async_streaming_result(conn, cleanup_query);
    
    // Process some data
    auto it = streaming_result.begin();
    co_await it.advance();
    
    // Early exit - automatic cleanup
    if (found_target) {
        break;
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
    
    auto result = conn.execute<UserDTO>(query);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::println("Query executed in {} ms", duration.count());
}
```

### Connection Pool Metrics

Monitor connection pool performance:

```cpp
auto stats = pool->get_statistics();
std::println("Active connections: {}", stats.active_connections);
std::println("Idle connections: {}", stats.idle_connections);
std::println("Total created: {}", stats.total_created);
std::println("Total destroyed: {}", stats.total_destroyed);
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
