# Asynchronous Database Connection Design

## Overview

This document outlines the design for SQLlib's asynchronous database connection API using C++20 coroutines and Boost.Asio. This implementation enables non-blocking database operations through modern, ergonomic coroutine syntax, allowing applications to maintain responsiveness while database operations proceed in the background.

## Design Goals

1. **Non-blocking Operations**: Execute database operations without blocking the calling thread
2. **Resource Efficiency**: Avoid thread-per-connection models in favor of asynchronous I/O
3. **Ergonomic API**: Leverage C++20 coroutines for clean, sequential-looking asynchronous code
4. **Composability**: Support composition of async operations for complex workflows
5. **Performance**: Minimize overhead for async operations
6. **Integration**: Seamlessly integrate with application's existing Boost.Asio event loop

## Architecture

### Core Components

#### AsyncConnection

```cpp
namespace sqllib {

class AsyncConnection {
public:
    // Factory methods
    static boost::asio::awaitable<AsyncConnection> create(
        const ConnectionString& conn_str, 
        boost::asio::io_context& io_context
    );
    
    static boost::asio::awaitable<AsyncConnection> create_sqlite(
        const std::string& filename, 
        boost::asio::io_context& io_context
    );
    
    static boost::asio::awaitable<AsyncConnection> create_mysql(
        const ConnectionParams& params, 
        boost::asio::io_context& io_context
    );
    
    static boost::asio::awaitable<AsyncConnection> create_postgresql(
        const ConnectionParams& params, 
        boost::asio::io_context& io_context
    );
    
    // Asynchronous operations
    template<QueryExprConcept QueryType>
    boost::asio::awaitable<ResultSet> execute(const QueryType& query);
    
    // Transaction management
    boost::asio::awaitable<AsyncTransaction> begin_transaction();
    
    // Connection-level operations
    boost::asio::awaitable<void> connect();
    boost::asio::awaitable<void> close();
    
    // Status methods
    bool is_connected() const;
    boost::asio::io_context& get_io_context() const noexcept;
    
private:
    std::unique_ptr<AsyncConnectionImpl> impl_;
    boost::asio::io_context& io_context_;
};

} // namespace sqllib
```

#### AsyncTransaction

```cpp
namespace sqllib {

class AsyncTransaction {
public:
    // Commit the transaction asynchronously
    boost::asio::awaitable<void> commit();
    
    // Rollback the transaction asynchronously
    boost::asio::awaitable<void> rollback();
    
    // Set transaction properties
    void set_isolation_level(Transaction::IsolationLevel level);
    
    // Auto-rollback on destruction if not committed
    ~AsyncTransaction();
    
private:
    AsyncConnection* conn_;
    bool committed_;
    Transaction::IsolationLevel isolation_level_;
};

} // namespace sqllib
```

#### AsyncConnectionPool

```cpp
namespace sqllib {

class PooledAsyncConnection {
public:
    // Delegate to underlying connection
    template<QueryExprConcept QueryType>
    boost::asio::awaitable<ResultSet> execute(const QueryType& query);
    
    boost::asio::awaitable<AsyncTransaction> begin_transaction();
    
    // Auto-return to pool on destruction
    ~PooledAsyncConnection();
    
private:
    AsyncConnectionPool* pool_;
    std::unique_ptr<AsyncConnection> conn_;
};

class AsyncConnectionPool {
public:
    AsyncConnectionPool(const ConnectionString& conn_str, 
                       boost::asio::io_context& io_context,
                       size_t min_pool_size, 
                       size_t max_pool_size);
    
    // Acquire a connection asynchronously
    boost::asio::awaitable<PooledAsyncConnection> acquire();
    
    // Pool management and statistics
    size_t get_active_connection_count() const;
    size_t get_idle_connection_count() const;
    boost::asio::awaitable<void> close_idle_connections();
    
private:
    ConnectionString conn_str_;
    boost::asio::io_context& io_context_;
    size_t min_pool_size_;
    size_t max_pool_size_;
    std::vector<std::unique_ptr<AsyncConnection>> connections_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};

} // namespace sqllib
```

### Coroutine Mechanism and Execution Context

SQLlib uses Boost.Asio's `awaitable<T>` as the primary coroutine return type, which integrates with Asio's execution model:

```cpp
namespace sqllib {

// Example of integrating with an application's io_context
boost::asio::awaitable<void> setup_database(boost::asio::io_context& io_context) {
    // Create connection using application's io_context
    auto conn = co_await AsyncConnection::create_postgresql({
        .host = "localhost",
        .user = "username",
        .password = "password",
        .database = "mydb"
    }, io_context);
    
    // Connection is ready to use with coroutines
    co_return;
}

} // namespace sqllib
```

### Coroutine-Based Operation Execution

#### Basic Query Execution

```cpp
namespace sqllib {

boost::asio::awaitable<void> query_users(AsyncConnection& conn) {
    User u;
    
    try {
        // Execute query with co_await
        ResultSet results = co_await conn.execute(
            Select(u.id, u.name).from(u).where(u.id > 10)
        );
        
        // Process results
        for (const auto& row : results) {
            std::cout << row.get<0>() << ": " << row.get<1>() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    co_return;
}

} // namespace sqllib
```

#### Transaction Example

```cpp
namespace sqllib {

boost::asio::awaitable<void> transfer_money(
    AsyncConnection& conn, 
    int from_account, 
    int to_account, 
    double amount
) {
    try {
        // Begin transaction with co_await
        auto txn = co_await conn.begin_transaction();
        
        Accounts accounts;
        
        // Execute first update
        co_await conn.execute(
            Update(accounts)
                .set(accounts.balance, accounts.balance - amount)
                .where(accounts.id == from_account)
        );
        
        // Execute second update
        co_await conn.execute(
            Update(accounts)
                .set(accounts.balance, accounts.balance + amount)
                .where(accounts.id == to_account)
        );
        
        // Commit transaction
        co_await txn.commit();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        // Transaction automatically rolled back by destructor
    }
    
    co_return;
}

} // namespace sqllib
```

#### Parallel Execution with Coroutines

```cpp
namespace sqllib {

boost::asio::awaitable<void> process_parallel_queries(AsyncConnection& conn) {
    User u;
    Product p;
    Order o;
    
    // Launch multiple coroutines in parallel
    auto users_future = conn.execute(Select(u.id, u.name).from(u).where(u.active == true));
    auto products_future = conn.execute(Select(p.id, p.name).from(p).where(p.stock > 0));
    auto orders_future = conn.execute(Select(o.id, o.date).from(o).where(o.status == "pending"));
    
    // Wait for all results
    auto [users, products, orders] = co_await boost::asio::join(
        std::move(users_future), 
        std::move(products_future),
        std::move(orders_future)
    );
    
    // Process all results
    process_users(users);
    process_products(products);
    process_orders(orders);
    
    co_return;
}

} // namespace sqllib
```

## Implementation Strategy

### Database-Specific Implementations

#### AsyncConnectionImpl Interface

```cpp
namespace sqllib::detail {

class AsyncConnectionImpl {
public:
    virtual ~AsyncConnectionImpl() = default;
    
    virtual boost::asio::awaitable<void> connect_impl() = 0;
    virtual boost::asio::awaitable<void> close_impl() = 0;
    virtual boost::asio::awaitable<ResultSet> execute_impl(
        const std::string& sql, 
        const std::vector<Parameter>& params
    ) = 0;
    virtual boost::asio::awaitable<void> begin_transaction_impl() = 0;
    virtual boost::asio::awaitable<void> commit_impl() = 0;
    virtual boost::asio::awaitable<void> rollback_impl() = 0;
    
    virtual bool is_connected_impl() const = 0;
};

} // namespace sqllib::detail
```

#### SQLite

SQLite doesn't have native async APIs. We'll implement asynchronous operations using Boost.Asio's thread pool:

```cpp
namespace sqllib::detail {

class AsyncSQLiteConnection : public AsyncConnectionImpl {
public:
    AsyncSQLiteConnection(const std::string& filename, boost::asio::io_context& io_context);
    
    boost::asio::awaitable<void> connect_impl() override;
    boost::asio::awaitable<void> close_impl() override;
    boost::asio::awaitable<ResultSet> execute_impl(
        const std::string& sql, 
        const std::vector<Parameter>& params
    ) override;
    boost::asio::awaitable<void> begin_transaction_impl() override;
    boost::asio::awaitable<void> commit_impl() override;
    boost::asio::awaitable<void> rollback_impl() override;
    
    bool is_connected_impl() const override;
    
private:
    sqlite3* db_;
    boost::asio::io_context& io_context_;
    bool is_connected_;
    
    // Offload SQLite operations to thread pool
    template<typename Func>
    boost::asio::awaitable<auto> offload_to_thread_pool(Func&& func);
};

// Implementation of thread pool offloading
template<typename Func>
boost::asio::awaitable<auto> AsyncSQLiteConnection::offload_to_thread_pool(Func&& func) {
    auto executor = co_await boost::asio::this_coro::executor;
    
    // Create the promise/future pair for the result
    using result_type = std::invoke_result_t<Func>;
    std::promise<result_type> promise;
    std::future<result_type> future = promise.get_future();
    
    // Post the work to Asio's thread pool
    boost::asio::post(
        io_context_,
        [func = std::forward<Func>(func), promise = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_same_v<result_type, void>) {
                    func();
                    promise.set_value();
                } else {
                    promise.set_value(func());
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        }
    );
    
    // Await the result
    co_return co_await [&future, executor]() -> boost::asio::awaitable<result_type> {
        // Wait for the future to be ready in a non-blocking way
        while (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            co_await boost::asio::post(executor, boost::asio::use_awaitable);
        }
        
        return future.get();
    }();
}

} // namespace sqllib::detail
```

#### MySQL

Using MySQL's native async API with Boost.Asio integration:

```cpp
namespace sqllib::detail {

class AsyncMySQLConnection : public AsyncConnectionImpl {
public:
    AsyncMySQLConnection(const ConnectionParams& params, boost::asio::io_context& io_context);
    
    boost::asio::awaitable<void> connect_impl() override;
    boost::asio::awaitable<void> close_impl() override;
    boost::asio::awaitable<ResultSet> execute_impl(
        const std::string& sql, 
        const std::vector<Parameter>& params
    ) override;
    boost::asio::awaitable<void> begin_transaction_impl() override;
    boost::asio::awaitable<void> commit_impl() override;
    boost::asio::awaitable<void> rollback_impl() override;
    
    bool is_connected_impl() const override;
    
private:
    MYSQL* conn_;
    boost::asio::io_context& io_context_;
    boost::asio::posix::stream_descriptor socket_descriptor_;
    bool is_connected_;
    
    // Helper for awaiting MySQL socket readiness
    boost::asio::awaitable<void> wait_for_socket(bool for_read);
};

// Wait for MySQL socket to be ready
boost::asio::awaitable<void> AsyncMySQLConnection::wait_for_socket(bool for_read) {
    auto executor = co_await boost::asio::this_coro::executor;
    
    if (for_read) {
        co_await socket_descriptor_.async_wait(
            boost::asio::posix::stream_descriptor::wait_read,
            boost::asio::use_awaitable
        );
    } else {
        co_await socket_descriptor_.async_wait(
            boost::asio::posix::stream_descriptor::wait_write,
            boost::asio::use_awaitable
        );
    }
    
    co_return;
}

} // namespace sqllib::detail
```

#### PostgreSQL

Using libpq's async API with Boost.Asio integration:

```cpp
namespace sqllib::detail {

class AsyncPostgreSQLConnection : public AsyncConnectionImpl {
public:
    AsyncPostgreSQLConnection(const ConnectionParams& params, boost::asio::io_context& io_context);
    
    boost::asio::awaitable<void> connect_impl() override;
    boost::asio::awaitable<void> close_impl() override;
    boost::asio::awaitable<ResultSet> execute_impl(
        const std::string& sql, 
        const std::vector<Parameter>& params
    ) override;
    boost::asio::awaitable<void> begin_transaction_impl() override;
    boost::asio::awaitable<void> commit_impl() override;
    boost::asio::awaitable<void> rollback_impl() override;
    
    bool is_connected_impl() const override;
    
private:
    PGconn* conn_;
    boost::asio::io_context& io_context_;
    boost::asio::posix::stream_descriptor socket_descriptor_;
    enum class State { IDLE, CONNECTING, EXECUTING, FETCHING_RESULTS };
    State state_ = State::IDLE;
    
    // Helper for awaiting PostgreSQL socket readiness
    boost::asio::awaitable<void> wait_for_socket_ready();
    boost::asio::awaitable<void> ensure_connection_ready();
};

// Wait for PostgreSQL socket to be ready
boost::asio::awaitable<void> AsyncPostgreSQLConnection::wait_for_socket_ready() {
    auto executor = co_await boost::asio::this_coro::executor;
    
    int socket_status = PQsocket(conn_);
    if (socket_status < 0) {
        throw ConnectionException("Invalid PostgreSQL connection socket");
    }
    
    PostgresPollingStatusType poll_status;
    
    do {
        if (state_ == State::CONNECTING) {
            poll_status = PQconnectPoll(conn_);
        } else {
            poll_status = PQisBusy(conn_) ? PGRES_POLLING_READING : PGRES_POLLING_OK;
        }
        
        switch (poll_status) {
            case PGRES_POLLING_READING:
                co_await socket_descriptor_.async_wait(
                    boost::asio::posix::stream_descriptor::wait_read,
                    boost::asio::use_awaitable
                );
                break;
                
            case PGRES_POLLING_WRITING:
                co_await socket_descriptor_.async_wait(
                    boost::asio::posix::stream_descriptor::wait_write,
                    boost::asio::use_awaitable
                );
                break;
                
            case PGRES_POLLING_FAILED:
                throw ConnectionException(PQerrorMessage(conn_));
                
            case PGRES_POLLING_OK:
                // Ready to proceed
                break;
        }
    } while (poll_status != PGRES_POLLING_OK);
    
    co_return;
}

} // namespace sqllib::detail
```

### Error Handling with Coroutines

Coroutines naturally propagate exceptions up the call stack, making error handling more intuitive:

```cpp
namespace sqllib {

boost::asio::awaitable<void> handle_query_with_error_handling(AsyncConnection& conn) {
    try {
        User u;
        
        // Execute query with co_await - exceptions are propagated naturally
        ResultSet results = co_await conn.execute(
            Select(u.id, u.name).from(u).where(u.id > 10)
        );
        
        // Process results
        for (const auto& row : results) {
            process_user_row(row);
        }
    } catch (const ConnectionException& e) {
        // Handle connection error
        std::cerr << "Connection error: " << e.what() << std::endl;
        co_await reconnect_database();
    } catch (const QueryException& e) {
        // Handle query error
        std::cerr << "Query error: " << e.what() << std::endl;
        std::cerr << "Failed query: " << e.get_query() << std::endl;
    } catch (const std::exception& e) {
        // Handle other errors
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    co_return;
}

} // namespace sqllib
```

## Advanced Features

### Connection Pooling with Coroutines

```cpp
namespace sqllib {

boost::asio::awaitable<void> handle_request_with_pool(
    AsyncConnectionPool& pool,
    int user_id
) {
    try {
        // Acquire connection from pool
        auto conn = co_await pool.acquire();
        
        // Use the connection
        User u;
        auto results = co_await conn.execute(
            Select(u.name, u.email).from(u).where(u.id == user_id)
        );
        
        // Process results
        if (results.has_rows() && results.next()) {
            auto name = results.get<0>();
            auto email = results.get<1>();
            
            // Use the user data
            co_await send_email(email, "Hello " + name);
        }
        
        // Connection automatically returned to pool when conn goes out of scope
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    co_return;
}

} // namespace sqllib
```

### Batch Operations with Coroutines

```cpp
namespace sqllib {

template<QueryExprConcept... QueryTypes>
class AsyncQueryBatch {
public:
    AsyncQueryBatch(QueryTypes... queries);
    
    boost::asio::awaitable<std::vector<ResultSet>> execute_all(AsyncConnection& conn);
    
    // Execute in parallel where possible 
    boost::asio::awaitable<std::vector<ResultSet>> execute_parallel(AsyncConnection& conn);
};

// Example usage
boost::asio::awaitable<void> process_batch(AsyncConnection& conn) {
    User u;
    Product p;
    
    AsyncQueryBatch batch(
        Select(u.id, u.name).from(u).where(u.active == true),
        Select(p.id, p.name, p.price).from(p).where(p.stock > 0)
    );
    
    try {
        // Execute batch and get all results
        auto results = co_await batch.execute_all(conn);
        
        // Process user results
        for (const auto& row : results[0]) {
            // Process user row
        }
        
        // Process product results
        for (const auto& row : results[1]) {
            // Process product row
        }
    } catch (const std::exception& e) {
        std::cerr << "Batch error: " << e.what() << std::endl;
    }
    
    co_return;
}

} // namespace sqllib
```

### Query Cancellation with Coroutines

```cpp
namespace sqllib {

class CancellableOperation {
public:
    void cancel() { 
        cancelled_ = true; 
        if (cancel_callback_) {
            cancel_callback_();
        }
    }
    
    bool is_cancelled() const { return cancelled_; }
    
    template<typename CancelFunc>
    void set_cancel_callback(CancelFunc&& func) {
        cancel_callback_ = std::forward<CancelFunc>(func);
    }
    
private:
    std::atomic<bool> cancelled_ = false;
    std::function<void()> cancel_callback_;
};

template<QueryExprConcept QueryType>
boost::asio::awaitable<std::pair<ResultSet, CancellableOperation>> execute_with_cancellation(
    AsyncConnection& conn,
    const QueryType& query
) {
    CancellableOperation cancel_op;
    
    // Create the operation 
    auto result_awaitable = conn.execute(query);
    
    // Set up cancellation
    cancel_op.set_cancel_callback([&conn]() {
        // Database-specific cancellation logic
        if (dynamic_cast<detail::AsyncPostgreSQLConnection*>(conn.get_impl())) {
            PQcancel(static_cast<detail::AsyncPostgreSQLConnection*>(conn.get_impl())->get_pgconn());
        } else if (dynamic_cast<detail::AsyncMySQLConnection*>(conn.get_impl())) {
            mysql_cancel_async_operation(static_cast<detail::AsyncMySQLConnection*>(conn.get_impl())->get_mysql());
        }
        // Note: SQLite doesn't have a clean way to cancel operations
    });
    
    // Execute the query
    ResultSet result = co_await result_awaitable;
    
    co_return std::make_pair(std::move(result), std::move(cancel_op));
}

// Example usage
boost::asio::awaitable<void> handle_search_with_timeout(
    AsyncConnection& conn,
    std::string search_term,
    std::chrono::seconds timeout
) {
    User u;
    
    // Create cancellation token source
    boost::asio::cancellation_token_source cts;
    
    // Start a timer for timeout
    auto timer = std::make_shared<boost::asio::steady_timer>(
        conn.get_io_context(), 
        timeout
    );
    
    // Create the operation with cancellation
    auto [op_result, cancel_op] = co_await execute_with_cancellation(
        conn,
        Select(u.id, u.name).from(u).where(like(u.name, "%" + search_term + "%"))
    );
    
    // Start the timer
    timer->async_wait(
        [&cancel_op](const boost::system::error_code& ec) {
            if (!ec) {
                cancel_op.cancel();
            }
        }
    );
    
    try {
        // If the query completes before timeout, process results
        if (!cancel_op.is_cancelled()) {
            timer->cancel();  // Cancel the timeout timer
            
            // Process results
            for (const auto& row : op_result) {
                std::cout << "Found user: " << row.get<1>() << std::endl;
            }
        } else {
            std::cout << "Search was cancelled due to timeout" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during search: " << e.what() << std::endl;
    }
    
    co_return;
}

} // namespace sqllib
```

## Implementation Phases

### Phase 1: Core AsyncConnection Interface
- Define the AsyncConnection and AsyncConnectionImpl interfaces with coroutine support
- Implement base mechanisms for translating to/from awaitable types

### Phase 2: SQLite Implementation
- Implement a thread-pool based approach for SQLite
- Create coroutine integration with thread pool

### Phase 3: Transaction Support
- Implement AsyncTransaction with commit/rollback operations
- Ensure proper RAII behavior with exceptions

### Phase 4: Native Database Async APIs
- Implement MySQL adapter using native async API and coroutines
- Implement PostgreSQL adapter using libpq async functions and coroutines

### Phase 5: Advanced Features
- AsyncConnectionPool implementation
- Batch operations
- Query cancellation support
- Timeout and parallel execution patterns

## Best Practices for Coroutine Usage

1. **Always annotate coroutine functions with `awaitable<T>`**:
   ```cpp
   boost::asio::awaitable<ResultSet> execute(const QueryType& query);
   ```

2. **Use `co_await` consistently**:
   ```cpp
   auto result = co_await conn.execute(query);
   ```

3. **Handle exceptions with try/catch inside coroutines**:
   ```cpp
   try {
       co_await async_operation();
   } catch (const std::exception& e) {
       // Handle error
   }
   ```

4. **Remember to use `co_return` instead of `return`**:
   ```cpp
   co_return result;
   ```

5. **Use `boost::asio::join` for waiting on multiple operations**:
   ```cpp
   auto [result1, result2] = co_await boost::asio::join(operation1(), operation2());
   ```

6. **Ensure proper cleanup with RAII even in coroutines**:
   ```cpp
   {
       auto conn = co_await pool.acquire();
       // Use conn
       // conn automatically returned to pool at end of scope
   }
   ```

7. **Don't forget `co_await this_coro::executor` when needed**:
   ```cpp
   auto executor = co_await boost::asio::this_coro::executor;
   ```
