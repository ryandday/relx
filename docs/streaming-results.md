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
#include <relx/connection/postgresql_connection.hpp>

Users users;

// Execute query and get lazy result set
auto result = conn.execute_lazy(
    relx::select(users.id, users.name, users.email)
        .from(users)
        .where(users.age > 25)
);

if (result) {
    // Iterate over lazy rows - no parsing happens yet
    for (const auto& lazy_row : *result) {
        // Data is parsed only when accessed
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
}
```

### Conditional Data Access

```cpp
// Only parse expensive columns when needed
for (const auto& lazy_row : *result) {
    auto user_id = lazy_row.get<int>("id");
    auto is_premium = lazy_row.get<bool>("is_premium");
    
    if (user_id && is_premium && *is_premium) {
        // Only parse large text fields for premium users
        auto bio = lazy_row.get<std::string>("bio");
        auto preferences = lazy_row.get<std::string>("preferences");
        
        process_premium_user(*user_id, bio.value_or(""), preferences.value_or(""));
    }
}
```

## Synchronous Streaming

Process results as they arrive from the database without loading everything into memory.

### Basic Streaming

```cpp
#include <relx/connection/postgresql_streaming_source.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>

// Define schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "active", bool> active;
    relx::column<Users, "created_at", std::string> created_at;
};

Users users;

// Build query using relx API
auto query = relx::select(users.id, users.name, users.email)
    .from(users)
    .where(users.active == true)
    .order_by(users.created_at);

// Create streaming result
auto streaming_result = relx::connection::create_streaming_result(conn, query);

// Process results one by one
// We use the for each function because 
streaming_result.for_each([](const auto& lazy_row) {
    auto id = lazy_row.get<int>("id");
    auto name = lazy_row.get<std::string>("name");
    auto email = lazy_row.get<std::string>("email");
    
    if (id && name && email) {
        process_user(*id, *name, *email);
    }
});
```

### Manual Streaming Iteration

```cpp
struct LargeTable {
    static constexpr auto table_name = "large_table";
    
    relx::column<LargeTable, "id", int, relx::primary_key> id;
    relx::column<LargeTable, "data", std::string> data;
    relx::column<LargeTable, "timestamp", std::string> timestamp;
};

LargeTable large_table;

// Build query using relx API
auto query = relx::select_all<LargeTable>()
    .from(large_table)
    .order_by(large_table.timestamp);

auto streaming_result = relx::connection::create_streaming_result(conn, query);

auto it = streaming_result.begin();
int processed_count = 0;

while (!it.is_at_end() && processed_count < 1000) {
    const auto& lazy_row = *it;
    
    // Process current row
    process_row(lazy_row);
    processed_count++;
    
    // Move to next row
    it.advance();
}

std::println("Processed {} rows", processed_count);
```

struct UsersWithScore {
    static constexpr auto table_name = "users";
    
    relx::column<UsersWithScore, "id", int, relx::primary_key> id;
    relx::column<UsersWithScore, "name", std::string> name;
    relx::column<UsersWithScore, "score", int> score;
    relx::column<UsersWithScore, "department_id", int> department_id;
};

### Streaming with Parameters

```cpp
UsersWithScore users;

// Build parameterized query using relx API
auto query = relx::select(users.id, users.name, users.score)
    .from(users)
    .where(users.department_id == department_id && users.score > minimum_score)
    .order_by(users.score.desc());

auto streaming_result = relx::connection::create_streaming_result(conn, query);

std::vector<int> high_scorers;
streaming_result.for_each([&high_scorers](const auto& lazy_row) {
    auto score = lazy_row.get<int>("score");
    if (score && *score > 95) {
        auto id = lazy_row.get<int>("id");
        if (id) {
            high_scorers.push_back(*id);
        }
    }
});
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

// Define schema
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::primary_key> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "profile_data", std::string> profile_data;
};

boost::asio::awaitable<void> process_users_async() {
    relx::connection::PostgreSQLAsyncConnection conn(io_context, connection_string);
    
    auto connect_result = co_await conn.connect();
    if (!connect_result) {
        std::println("Connection failed: {}", connect_result.error().message);
        co_return;
    }
    
    Users users;
    
    // Build query using relx API
    auto query = relx::select(users.id, users.name, users.email, users.profile_data)
        .from(users)
        .order_by(users.id);
    
    // Create async streaming result
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
    
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

struct MassiveTable {
    static constexpr auto table_name = "massive_table";
    
    relx::column<MassiveTable, "id", int, relx::primary_key> id;
    relx::column<MassiveTable, "data", std::string> data;
    relx::column<MassiveTable, "timestamp", std::string> timestamp;
};

### Manual Async Iteration

```cpp
boost::asio::awaitable<void> process_with_backpressure() {
    MassiveTable massive_table;
    
    // Build query using relx API
    auto query = relx::select_all<MassiveTable>()
        .from(massive_table)
        .order_by(massive_table.timestamp);
    
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
    
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

struct UserActivities {
    static constexpr auto table_name = "user_activities";
    
    relx::column<UserActivities, "id", int, relx::primary_key> id;
    relx::column<UserActivities, "user_id", int> user_id;
    relx::column<UserActivities, "activity_type", std::string> activity_type;
    relx::column<UserActivities, "timestamp", std::string> timestamp;
    relx::column<UserActivities, "details", std::string> details;
};

### Async Streaming with Parameters

```cpp
boost::asio::awaitable<void> stream_user_activities(int user_id, const std::string& start_date) {
    UserActivities user_activities;
    
    // Build parameterized query using relx API
    auto query = relx::select(user_activities.activity_type, user_activities.timestamp, user_activities.details)
        .from(user_activities)
        .where(user_activities.user_id == user_id && user_activities.timestamp >= start_date)
        .order_by(user_activities.timestamp);
    
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
    
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
    Users users;
    
    // Build query using relx API
    auto query = relx::select_all<Users>().from(users);
    
    // Streaming result automatically cleans up when iteration completes
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
    
    co_await streaming_result.for_each([](const auto& lazy_row) {
        // Process row
    });
    
    // Connection state is automatically reset for new operations
} // streaming_result destructor ensures cleanup
```

### Automatic Cleanup on Early Exit

```cpp
{
    LargeTable large_table;
    
    // Build query using relx API
    auto query = relx::select_all<LargeTable>().from(large_table);
    
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
    
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
        Users users;
        
        // Build query using relx API
        auto query = relx::select_all<Users>().from(users);
        
        auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
        
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
auto regular_result = conn.execute(
    relx::select_all<Users>().from(users).limit(1000000)
);
// Memory usage: ~100MB for 1M rows

// Streaming approach - constant memory usage
Users users;
auto query = relx::select_all<Users>().from(users).limit(1000000);
auto streaming_result = relx::connection::create_streaming_result(conn, query);
// Memory usage: ~1KB regardless of result size
```

### Processing Speed Optimization

```cpp
// Batch processing for better performance
boost::asio::awaitable<void> optimized_streaming() {
    LargeTable large_table;
    
    // Build query using relx API
    auto query = relx::select(large_table.id, large_table.data)
        .from(large_table)
        .order_by(large_table.id);
    
    auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
    
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

struct RecentEvents {
    static constexpr auto table_name = "recent_events";
    
    relx::column<RecentEvents, "id", int, relx::primary_key> id;
    relx::column<RecentEvents, "timestamp", std::string> timestamp;
    relx::column<RecentEvents, "event_data", std::string> event_data;
};

struct HugeTable {
    static constexpr auto table_name = "huge_table";
    relx::column<HugeTable, "id", int, relx::primary_key> id;
    relx::column<HugeTable, "data", std::string> data;
};

### Streaming Guidelines

```cpp
// ✅ Good: Order by indexed columns for efficient streaming
Users users;
auto good_query = relx::select_all<Users>()
    .from(users)
    .order_by(users.id);  // id is primary key
auto streaming_result = relx::connection::create_streaming_result(conn, good_query);

// ✅ Good: Use LIMIT with streaming for bounded processing
RecentEvents recent_events;
auto limited_query = relx::select_all<RecentEvents>()
    .from(recent_events)
    .order_by(recent_events.timestamp.desc())
    .limit(10000);
auto streaming_result2 = relx::connection::create_streaming_result(conn, limited_query);

// ❌ Avoid: Unordered streaming of very large tables
HugeTable huge_table;
auto bad_query = relx::select_all<HugeTable>().from(huge_table);  // No ORDER BY
// This might be inefficient

// ✅ Good: Filter at database level
auto filtered_query = relx::select_all<Users>()
    .from(users)
    .where(users.active == true && users.last_login > cutoff_date)
    .order_by(users.id);
auto streaming_result3 = relx::connection::create_streaming_result(conn, filtered_query);
```

## Error Handling

struct UnreliableSource {
    static constexpr auto table_name = "unreliable_source";
    
    relx::column<UnreliableSource, "id", int, relx::primary_key> id;
    relx::column<UnreliableSource, "data", std::string> data;
};

### Handling Streaming Errors

```cpp
boost::asio::awaitable<void> robust_streaming() {
    try {
        UnreliableSource unreliable_source;
        
        // Build query using relx API
        auto query = relx::select(unreliable_source.id, unreliable_source.data)
            .from(unreliable_source)
            .order_by(unreliable_source.id);
        
        auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
        
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

struct CriticalData {
    static constexpr auto table_name = "critical_data";
    
    relx::column<CriticalData, "id", int, relx::primary_key> id;
    relx::column<CriticalData, "data", std::string> data;
    relx::column<CriticalData, "timestamp", std::string> timestamp;
};

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
            
            CriticalData critical_data;
            
            // Build query using relx API
            auto query = relx::select_all<CriticalData>()
                .from(critical_data)
                .order_by(critical_data.id);
            
            auto streaming_result = relx::connection::create_async_streaming_result(conn, query);
            
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
- [Result Parsing](result-parsing.html) - Basic result processing
- [Advanced Examples](advanced-examples.html) - Complex streaming scenarios  
- [Performance Guide](performance.html) - Optimization tips
- [Error Handling](error-handling.html) - Robust error management 