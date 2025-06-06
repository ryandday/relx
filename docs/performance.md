---
layout: default
title: Performance Guide
nav_order: 7
---

# Performance Guide

This guide covers performance best practices when using relx for building and executing SQL queries.

## Table of Contents

- [Runtime Performance](#runtime-performance)
- [Connection Management](#connection-management)
- [Query Optimization](#query-optimization)
- [Memory Management](#memory-management)
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
