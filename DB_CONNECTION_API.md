# Database Connection API Plan

## Overview
This document outlines the design for SQLlib's database connection API. The connection layer serves as the interface between our type-safe SQL query builders and actual database engines, providing a consistent approach to connection management, transaction control, and statement execution.

## Core Design Goals
1. **Abstraction**: Provide a unified interface for multiple database backends
2. **Type Safety**: Maintain type safety from query creation through execution
3. **Resource Management**: RAII-based connection and transaction management
4. **Performance**: Efficient statement preparation and execution
5. **Extensibility**: Easy addition of new database backend support

## Architecture

### Connection Management

#### Connection Interface
```cpp
namespace sqllib {

/// @brief Generic connection interface supporting different database backends
class Connection {
public:
    /// @brief Connection creation factory methods
    static Connection create(const ConnectionString& conn_str);
    static Connection create_sqlite(const std::string& filename);
    static Connection create_mysql(const ConnectionParams& params);
    static Connection create_postgresql(const ConnectionParams& params);
    
    /// @brief Connection state management
    bool is_connected() const;
    void reconnect();
    void close();
    
    /// @brief Statement execution with type safety
    template<QueryExprConcept QueryType>
    ResultSet execute(const QueryType& query);
    
    /// @brief Direct SQL execution (escape hatch for complex queries)
    ResultSet execute_raw(const std::string& sql, const std::vector<Parameter>& params = {});
    
    /// @brief Transaction control
    Transaction begin_transaction();
    void commit();
    void rollback();
    
    /// @brief Connection configuration
    void set_timeout(std::chrono::seconds timeout);
    void set_option(ConnectionOption option, const std::any& value);
    
private:
    std::unique_ptr<ConnectionImpl> impl_;
    bool is_connected_;
};

} // namespace sqllib
```

#### Connection Pooling
```cpp
namespace sqllib {

/// @brief Optional connection pooling
class ConnectionPool {
public:
    ConnectionPool(const ConnectionString& conn_str, size_t min_pool_size, size_t max_pool_size);
    
    /// @brief Get a connection from the pool
    PooledConnection acquire();
    
    /// @brief Connection pool stats
    size_t get_active_connection_count() const;
    size_t get_idle_connection_count() const;
    
    /// @brief Pool management
    void set_max_pool_size(size_t size);
    void close_idle_connections();
    
private:
    ConnectionString conn_str_;
    size_t min_pool_size_;
    size_t max_pool_size_;
    std::vector<std::unique_ptr<Connection>> connections_;
};

/// @brief RAII wrapper for a pooled connection
class PooledConnection {
public:
    /// @brief Pooled connection behaves like a regular connection
    template<QueryExprConcept QueryType>
    ResultSet execute(const QueryType& query);
    
    Transaction begin_transaction();
    
    /// @brief Automatically returns to pool when destroyed
    ~PooledConnection();
    
private:
    ConnectionPool* pool_;
    std::unique_ptr<Connection> conn_;
};

} // namespace sqllib
```

### Transaction Management

#### RAII Transaction
```cpp
namespace sqllib {

/// @brief RAII-based transaction management
class Transaction {
public:
    /// @brief Transaction options
    enum class IsolationLevel {
        ReadUncommitted,
        ReadCommitted,
        RepeatableRead,
        Serializable
    };
    
    /// @brief Transaction control
    void commit();
    void rollback();
    
    /// @brief Auto-rollback on destruction if not committed
    ~Transaction();
    
    void set_isolation_level(IsolationLevel level);
    
private:
    Connection* conn_;
    bool committed_;
    IsolationLevel isolation_level_;
};

/// @brief Example usage
void transfer_money(Connection& conn, int from_account, int to_account, double amount) {
    auto txn = conn.begin_transaction();
    try {
        // Perform operations using the same connection
        Accounts accounts;
        conn.execute(Update(accounts).set(accounts.balance, accounts.balance - amount).where(accounts.id == from_account));
        conn.execute(Update(accounts).set(accounts.balance, accounts.balance + amount).where(accounts.id == to_account));
        
        txn.commit();
    } catch (...) {
        // Transaction automatically rolled back by destructor
        throw;
    }
}

} // namespace sqllib
```

### Statement Execution

#### Statement Preparation
```cpp
namespace sqllib {

/// @brief Internal prepared statement handling
class PreparedStatement {
public:
    /// @brief Parameter binding
    template<typename T>
    void bind_parameter(size_t index, const T& value);
    
    /// @brief Execution
    ResultSet execute();
    
    /// @brief Statement reuse
    void reset();
    
    /// @brief Resource cleanup
    ~PreparedStatement();
    
private:
    // Created internally by Connection::execute
    PreparedStatement(Connection& conn, const std::string& sql);
    
    Connection* conn_;
    std::string sql_;
    void* stmt_handle_;
};

} // namespace sqllib
```

#### Parameter Binding
```cpp
namespace sqllib {

/// @brief How a type-safe query gets executed
template<QueryExprConcept QueryType>
ResultSet Connection::execute(const QueryType& query) {
    // Convert QueryType to SQL string and parameter list
    auto [sql, params] = query.to_sql_and_params();
    
    // Prepare statement
    PreparedStatement stmt(*this, sql);
    
    // Bind parameters with correct types
    for (size_t i = 0; i < params.size(); ++i) {
        params[i].bind_to(stmt, i);
    }
    
    // Execute and return results
    return stmt.execute();
}

} // namespace sqllib
```

### Type-Safe Result Processing

#### ResultSet Interface
```cpp
namespace sqllib {

/// @brief Type-safe result set container
class ResultSet {
public:
    /// @brief Result set metadata
    size_t get_column_count() const;
    std::string get_column_name(size_t index) const;
    ColumnType get_column_type(size_t index) const;
    
    /// @brief Row iteration
    bool next();
    bool has_rows() const;
    size_t get_row_count() const;
    
    /// @brief Type-safe value access
    template<size_t Index>
    auto get() const;
    
    template<auto MemberPtr>
    auto get() const;
    
    /// @brief Optional value handling for NULLs
    template<auto MemberPtr>
    std::optional<std::remove_reference_t<decltype(std::declval<typename MemberPtrTraits<decltype(MemberPtr)>::ClassType>().*MemberPtr)>> 
    get_optional() const;
    
    /// @brief Transformation helpers
    template<typename T, typename FieldMapper>
    std::vector<T> transform(const FieldMapper& mapper) const;
    
    /// @brief C++17 structured binding support
    template<typename... Types>
    ResultView<Types...> as() const;
    
private:
    std::unique_ptr<ResultSetImpl> impl_;
    size_t current_row_;
};

} // namespace sqllib
```

### Database-Specific Adaptors

#### SQLite Adaptor
```cpp
namespace sqllib::detail {

/// @brief SQLite-specific connection implementation
class SQLiteConnection : public ConnectionImpl {
public:
    SQLiteConnection(const std::string& filename);
    
    ResultSet execute_impl(const std::string& sql, const std::vector<Parameter>& params) override;
    void begin_transaction_impl() override;
    void commit_impl() override;
    void rollback_impl() override;
    
private:
    sqlite3* db_;
    bool is_open_;
};

} // namespace sqllib::detail
```

#### MySQL Adaptor
```cpp
namespace sqllib::detail {

/// @brief MySQL-specific connection implementation
class MySQLConnection : public ConnectionImpl {
public:
    MySQLConnection(const ConnectionParams& params);
    
    ResultSet execute_impl(const std::string& sql, const std::vector<Parameter>& params) override;
    void begin_transaction_impl() override;
    void commit_impl() override;
    void rollback_impl() override;
    
private:
    MYSQL* conn_;
    bool is_connected_;
};

} // namespace sqllib::detail
```

## Implementation Strategy

### Phase 1: Core Connection Interface
- Define the Connection interface and basic implementations
- Implement SQLite backend as reference implementation
- Create the statement preparation and execution flow

### Phase 2: Transaction Support
- Implement RAII Transaction class
- Add transaction isolation level support
- Ensure proper error handling and automatic rollback

### Phase 3: Result Processing
- Implement ResultSet with type-safe access methods
- Add transformation utilities
- Support C++17 structured bindings

### Phase 4: Additional Database Backends
- Implement MySQL adaptor
- Implement PostgreSQL adaptor
- Create a pluggable backend system

### Phase 5: Advanced Features
- Connection pooling
- Asynchronous query execution
- Batch operations
- Performance tuning

## Usage Examples

### Basic Connection and Query
```cpp
namespace sqllib {

// Create a connection
auto conn = Connection::create_sqlite("database.db");

// Define schema
struct User : Table<"users"> {
    Column<"id", int> id;
    Column<"name", std::string> name;
    
    PrimaryKey<&User::id> pk;
};

// Execute a query
User u;
auto results = conn.execute(Select(u.id, u.name).from(u).where(u.id > 10));

// Process results
for (const auto& row : results) {
    int id = row.get<&User::id>();
    std::string name = row.get<&User::name>();
    std::cout << id << ": " << name << std::endl;
}

} // namespace sqllib
```

### Transaction with Error Handling
```cpp
namespace sqllib {

Connection conn = Connection::create_mysql({
    .host = "localhost",
    .user = "username",
    .password = "password",
    .database = "mydb"
});

try {
    auto txn = conn.begin_transaction();
    txn.set_isolation_level(Transaction::IsolationLevel::Serializable);
    
    // Execute multiple queries in the transaction
    conn.execute(/* ... */);
    conn.execute(/* ... */);
    
    txn.commit();
} catch (const DatabaseException& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
    // Transaction automatically rolled back
}

} // namespace sqllib
```

### Connection Pooling
```cpp
namespace sqllib {

// Create a connection pool
ConnectionPool pool("mysql://username:password@localhost/mydb", 5, 20);

// Function that needs a database connection
void process_user(int user_id) {
    // Get connection from pool
    auto conn = pool.acquire();
    
    // Use the connection
    User u;
    auto results = conn.execute(Select(u.name).from(u).where(u.id == user_id));
    
    // Connection automatically returned to pool when conn goes out of scope
}

} // namespace sqllib
```

## Error Handling Strategy

### Exception Hierarchy
```cpp
namespace sqllib {

/// @brief Base exception for all database errors
class DatabaseException : public std::runtime_error {
public:
    DatabaseException(const std::string& message);
};

/// @brief Exception for connection related errors
class ConnectionException : public DatabaseException {
public:
    ConnectionException(const std::string& message);
};

/// @brief Exception for query execution errors
class QueryException : public DatabaseException {
public:
    QueryException(const std::string& message, const std::string& query);
    const std::string& get_query() const;
    
private:
    std::string query_;
};

/// @brief Exception for transaction errors
class TransactionException : public DatabaseException {
public:
    TransactionException(const std::string& message);
};

} // namespace sqllib
```

### Error Information
```cpp
namespace sqllib {

/// @brief Detailed error information structure
struct DatabaseError {
    int code;
    std::string message;
    std::string sql_state;  // For SQL standard error codes
};

DatabaseError Connection::get_last_error() const;

} // namespace sqllib
