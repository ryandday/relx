# Streaming Results

relx provides advanced streaming capabilities for efficient processing of large result sets and memory-efficient lazy parsing. This guide covers both synchronous and asynchronous streaming options.

## Table of Contents

- [Overview](#overview)
- [Lazy Result Parsing](#lazy-result-parsing)
- [Synchronous Streaming](#synchronous-streaming)
- [Asynchronous Streaming](#asynchronous-streaming)
- [Automatic Resource Management](#automatic-resource-management)
- [Performance Considerations](#performance-considerations)
- [Best Practices](#best-practices)
- [Error Handling](#error-handling)

## Overview

Streaming allows you to process query results row-by-row without loading the entire result set into memory. This is especially valuable for:

- **Large result sets** that don't fit in memory
- **Real-time processing** scenarios
- **Memory-efficient applications**
- **Long-running data processing tasks**

relx provides three main approaches:

1. **Lazy Result Parsing** - Load the raw data all at once, but defer parsing until data is accessed
2. **Synchronous Streaming** - Process results one row at a time
3. **Asynchronous Streaming** - Non-blocking streaming with coroutines

## Lazy Result Parsing

Lazy parsing defers type conversion and parsing until you actually access the data, reducing memory usage and improving performance for partial result processing.

### Basic Lazy Results

```cpp
#include <relx/results/lazy_result.hpp>

auto lazy_result = relx::result::parse_lazy(query, std::move(raw_results));

// Iterating parses row boundaries; values are converted only when asked for
for (const auto& lazy_row : lazy_result) {
    auto id = lazy_row.get<int>("id");
    auto name = lazy_row.get<std::string>("name");

    if (id && name) {
        std::println("User {}: {}", *id, *name);

        // Only parse email if we need it
        if (*id > 100) {
            auto email = lazy_row.get<std::string>("email");
            if (email) {
                std::println("  Email: {}", *email);
            }
        }
    }
}
```

Lazy parsing still holds the whole raw result in memory — it saves conversion work, not bytes. The
streaming sections below are what keep memory constant. See
[Result Parsing](result-parsing.md#lazy-result-parsing) for more.

## Synchronous Streaming

Process results as they arrive from the database without loading everything into memory.

### Basic Streaming

```cpp
#include <relx/connection/postgresql_streaming_source.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>

// clang-format off
struct [[=relx::table("users")]] Users {
    [[=relx::ann::pk]] int id;
    std::string name;
    std::string email;
    bool active;
    std::string created_at;
};
// clang-format on
inline constexpr auto users = relx::t<Users>;

auto query = relx::select(users.id, users.name, users.email)
    .from(users)
    .where(users.active == true)
    .order_by(users.created_at);

// Streaming takes the query object directly; its SQL and bind parameters are
// forwarded (as text), and `?` placeholders are rewritten to $1, $2, ... for libpq
auto streaming_result = relx::connection::create_streaming_result(conn, query);

// A synchronous streaming result is an input range: each ++ pulls the next row
for (const auto& lazy_row : streaming_result) {
    auto id = lazy_row.get<int>("id");
    auto name = lazy_row.get<std::string>("name");
    auto email = lazy_row.get<std::string>("email");

    if (id && name && email) {
        process_user(*id, *name, *email);
    }
}
```

The synchronous result set also has `for_each` (callback returns void, or bool where `true` stops
the iteration) and manual iterator control via `it.advance()` / `it.is_at_end()`, mirroring the
async result set below. One semantic difference: the sync `begin()` is already positioned on the
first row, so manual loops advance *after* processing a row, while the async iterator needs a
`co_await it.advance()` before the first row.

A row pull can only observe "no more rows", so after iterating, `streaming_result.last_error()`
distinguishes a failed query from a genuinely empty result.

### Manual Streaming Iteration

```cpp
// clang-format off
struct [[=relx::table("large_table")]] LargeTable {
    [[=relx::ann::pk]] int id;
    std::string data;
    std::string timestamp;
};
// clang-format on
inline constexpr auto large_table = relx::t<LargeTable>;

auto query = relx::select_all(large_table).order_by(large_table.timestamp);

auto streaming_result = relx::connection::create_streaming_result(conn, query);

int processed_count = 0;
for (auto it = streaming_result.begin(); it != streaming_result.end(); ++it) {
    process_row(*it);
    if (++processed_count >= 1000) {
        break;  // Early exit; the destructor resets the connection state
    }
}

std::println("Processed {} rows", processed_count);
```

### Streaming with Parameters

```cpp
// clang-format off
struct [[=relx::table("users")]] UsersWithScore {
    [[=relx::ann::pk]] int id;
    std::string name;
    int score;
    int department_id;
};
// clang-format on
inline constexpr auto scored_users = relx::t<UsersWithScore>;

auto query = relx::select(scored_users.id, scored_users.name, scored_users.score)
    .from(scored_users)
    .where(scored_users.department_id == department_id && scored_users.score > minimum_score)
    .order_by(relx::desc(scored_users.score));

// The query's bind parameters travel with it, in placeholder order
auto streaming_result = relx::connection::create_streaming_result(conn, query);

std::vector<int> high_scorers;
for (const auto& lazy_row : streaming_result) {
    auto score = lazy_row.get<int>("score");
    if (score && *score > 95) {
        auto id = lazy_row.get<int>("id");
        if (id) {
            high_scorers.push_back(*id);
        }
    }
}
```

## Asynchronous Streaming

For high-performance applications, use asynchronous streaming with Boost.Asio coroutines.

### Basic Async Streaming

```cpp
#include <relx/connection/postgresql_async_streaming_source.hpp>
#include <relx/connection/postgresql_async_connection.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <boost/asio.hpp>

// clang-format off
struct [[=relx::table("users")]] Users {
    [[=relx::ann::pk]] int id;
    std::string name;
    std::string email;
    std::string profile_data;
};
// clang-format on
inline constexpr auto users = relx::t<Users>;

boost::asio::awaitable<void> process_users_async() {
    relx::connection::PostgreSQLAsyncConnection conn(io_context, connection_string);

    auto connect_result = co_await conn.connect();
    if (!connect_result) {
        std::println("Connection failed: {}", connect_result.error().message);
        co_return;
    }

    auto query = relx::select(users.id, users.name, users.email, users.profile_data)
        .from(users)
        .order_by(users.id);

    auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
    
    // Async iteration with for_each
    co_await streaming_result.for_each([](const auto& lazy_row) {
        auto id = lazy_row.get<int>("id");
        auto name = lazy_row.get<std::string>("name");
        
        if (id && name) {
            std::println("Processing user {}: {}", *id, *name);
        }
    });
    
    co_await conn.disconnect();
}

// Run the coroutine
boost::asio::io_context io_context;
boost::asio::co_spawn(io_context, process_users_async(), boost::asio::detached);
io_context.run();
```

### Manual Async Iteration

`for_each` covers most cases; manual iteration is for when you need to interleave your own async
work between rows.

```cpp
// clang-format off
struct [[=relx::table("massive_table")]] MassiveTable {
    [[=relx::ann::pk]] int id;
    std::string data;
    std::string timestamp;
};
// clang-format on
inline constexpr auto massive_table = relx::t<MassiveTable>;

boost::asio::awaitable<void> process_with_backpressure() {
    auto query = relx::select_all(massive_table).order_by(massive_table.timestamp);

    auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
    
    auto it = streaming_result.begin();
    int batch_size = 0;
    const int MAX_BATCH_SIZE = 100;
    
    // Advance to first row
    co_await it.advance();
    
    while (!it.is_at_end()) {
        const auto& lazy_row = *it;
        
        // Process row
        process_row(lazy_row);
        batch_size++;
        
        // Implement backpressure - pause every 100 rows
        if (batch_size >= MAX_BATCH_SIZE) {
            std::println("Processed {} rows, taking a break...", batch_size);
            
            // Simulate backpressure handling
            boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(10));
            co_await timer.async_wait(boost::asio::use_awaitable);
            
            batch_size = 0;
        }
        
        co_await it.advance();
    }
}
```

### Async Streaming with Parameters

```cpp
// clang-format off
struct [[=relx::table("user_activities")]] UserActivities {
    [[=relx::ann::pk]] int id;
    int user_id;
    std::string activity_type;
    std::string timestamp;
    std::string details;
};
// clang-format on
inline constexpr auto user_activities = relx::t<UserActivities>;

boost::asio::awaitable<void> stream_user_activities(int user_id, const std::string& start_date) {
    auto query = relx::select(user_activities.activity_type, user_activities.timestamp,
                              user_activities.details)
        .from(user_activities)
        .where(user_activities.user_id == user_id && user_activities.timestamp >= start_date)
        .order_by(user_activities.timestamp);

    auto streaming_result = relx::connection::create_async_streaming_result(
        conn, query.to_sql(), user_id, start_date);
    
    int activity_count = 0;
    co_await streaming_result.for_each([&activity_count](const auto& lazy_row) {
        auto activity_type = lazy_row.get<std::string>("activity_type");
        auto timestamp = lazy_row.get<std::string>("timestamp");
        
        if (activity_type && timestamp) {
            std::println("{}: {} activity at {}", 
                activity_count++, *activity_type, *timestamp);
        }
    });
    
    std::println("Total activities processed: {}", activity_count);
}
```

## Automatic Resource Management

relx provides RAII-based automatic resource management for streaming operations.

### Automatic Cleanup on Completion

```cpp
{
    auto query = relx::select_all(users);

    // Streaming result automatically cleans up when iteration completes
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
    
    co_await streaming_result.for_each([](const auto& lazy_row) {
        // Process row
    });
    
    // Connection state is automatically reset for new operations
} // streaming_result destructor ensures cleanup
```

### Automatic Cleanup on Early Exit

```cpp
{
    auto query = relx::select_all(large_table);

    auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
    
    auto it = streaming_result.begin();
    co_await it.advance();
    
    // Find first matching record and exit early
    while (!it.is_at_end()) {
        const auto& lazy_row = *it;
        auto id = lazy_row.get<int>("id");
        
        if (id && *id == target_id) {
            // Found what we're looking for
            break; // Early exit
        }
        
        co_await it.advance();
    }
    
    // streaming_result destructor automatically cleans up connection state
    // even though we didn't complete the full iteration
}

// Connection is ready for new operations immediately
```

### Exception Safety

```cpp
boost::asio::awaitable<void> safe_streaming_processing() {
    try {
        auto query = relx::select_all(users);

        auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
        
        co_await streaming_result.for_each([](const auto& lazy_row) {
            // This might throw an exception
            risky_processing(lazy_row);
        });
        
    } catch (const std::exception& e) {
        std::println("Error during streaming: {}", e.what());
        // streaming_result destructor still cleans up properly
    }
    
    // Connection is ready for new operations
}
```

## Performance Considerations

### Memory Usage Comparison

```cpp
// Traditional approach - loads all data into memory
auto regular_result = conn.execute(relx::select_all(users).limit(1000000));
// Memory usage: ~100MB for 1M rows

// Streaming approach - constant memory usage
auto query = relx::select_all(users).limit(1000000);
auto streaming_result = relx::connection::create_streaming_result(conn, query.to_sql(), 1000000);
// Memory usage: ~1KB regardless of result size
```

### Processing Speed Optimization

```cpp
// Batch processing for better performance
boost::asio::awaitable<void> optimized_streaming() {
    auto query = relx::select(large_table.id, large_table.data)
        .from(large_table)
        .order_by(large_table.id);

    auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
    
    std::vector<ProcessingData> batch;
    const size_t BATCH_SIZE = 1000;
    
    co_await streaming_result.for_each([&batch](const auto& lazy_row) {
        auto id = lazy_row.get<int>("id");
        auto data = lazy_row.get<std::string>("data");
        
        if (id && data) {
            batch.emplace_back(*id, *data);
            
            // Process in batches for better performance
            if (batch.size() >= BATCH_SIZE) {
                process_batch(batch);
                batch.clear();
            }
        }
    });
    
    // Process remaining items
    if (!batch.empty()) {
        process_batch(batch);
    }
}
```

## Best Practices

### Use Streaming When

✅ **Processing large result sets** (>10,000 rows)  
✅ **Memory is limited**  
✅ **You need to process results in real-time**  
✅ **Results don't need to be kept in memory**  
✅ **Early termination is possible**  

### Use Regular Results When

✅ **Small result sets** (<1,000 rows)  
✅ **Need random access to rows**  
✅ **Multiple passes over data are required**  
✅ **Results need to be sorted/filtered in application**  

### Streaming Guidelines

```cpp
// clang-format off
struct [[=relx::table("recent_events")]] RecentEvents {
    [[=relx::ann::pk]] int id;
    std::string timestamp;
    std::string event_data;
};

struct [[=relx::table("huge_table")]] HugeTable {
    [[=relx::ann::pk]] int id;
    std::string data;
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

// ✅ Good: Filter at database level
auto filtered_query = relx::select_all(users)
    .where(users.active == true)
    .order_by(users.id);
auto streaming_result3 = relx::connection::create_streaming_result(
    conn, filtered_query.to_sql(), true);
```

## Error Handling

### Handling Streaming Errors

```cpp
// clang-format off
struct [[=relx::table("unreliable_source")]] UnreliableSource {
    [[=relx::ann::pk]] int id;
    std::string data;
};
// clang-format on
inline constexpr auto unreliable_source = relx::t<UnreliableSource>;

boost::asio::awaitable<void> robust_streaming() {
    try {
        auto query = relx::select(unreliable_source.id, unreliable_source.data)
            .from(unreliable_source)
            .order_by(unreliable_source.id);

        auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
        
        int success_count = 0;
        int error_count = 0;
        
        co_await streaming_result.for_each([&](const auto& lazy_row) {
            try {
                auto id = lazy_row.get<int>("id");
                auto data = lazy_row.get<std::string>("data");
                
                if (id && data) {
                    process_data(*id, *data);
                    success_count++;
                } else {
                    std::println("Data parsing error for row");
                    error_count++;
                }
                
            } catch (const std::exception& e) {
                std::println("Processing error: {}", e.what());
                error_count++;
            }
        });
        
        std::println("Completed: {} successful, {} errors", success_count, error_count);
        
    } catch (const std::exception& e) {
        std::println("Streaming failed: {}", e.what());
    }
}
```

### Connection Error Recovery

```cpp
boost::asio::awaitable<void> streaming_with_reconnect() {
    int retry_count = 0;
    const int MAX_RETRIES = 3;
    
    while (retry_count < MAX_RETRIES) {
        try {
            auto connect_result = co_await conn.connect();
            if (!connect_result) {
                throw std::runtime_error(connect_result.error().message);
            }
            
            auto query = relx::select_all(users).order_by(users.id);

            auto streaming_result = relx::connection::create_async_streaming_result(conn, query.to_sql());
            
            co_await streaming_result.for_each([](const auto& lazy_row) {
                // Process row
            });
            
            break; // Success - exit retry loop
            
        } catch (const std::exception& e) {
            retry_count++;
            std::println("Streaming attempt {} failed: {}", retry_count, e.what());
            
            if (retry_count < MAX_RETRIES) {
                std::println("Retrying in 5 seconds...");
                boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
                timer.expires_after(std::chrono::seconds(5));
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
        }
    }
    
    if (retry_count >= MAX_RETRIES) {
        std::println("All streaming attempts failed");
    }
}
```

---

For more information, see:
- [Result Parsing](result-parsing.md) - Basic result processing
- [Performance Guide](performance.md) - Optimization tips
- [Error Handling](error-handling.md) - Robust error management 