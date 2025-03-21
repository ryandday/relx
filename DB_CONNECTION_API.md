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
// Generic connection interface supporting different backends
class Connection {
public:
    // Connection creation factory methods
    static Connection Create(const ConnectionString& connStr);
    static Connection CreateSQLite(const std::string& filename);
    static Connection CreateMySQL(const ConnectionParams& params);
    static Connection CreatePostgreSQL(const ConnectionParams& params);
    
    // Connection state management
    bool IsConnected() const;
    void Reconnect();
    void Close();
    
    // Statement execution
    template<typename QueryType>
    ResultSet Execute(const QueryType& query);
    
    // Direct SQL execution (escape hatch for complex queries)
    ResultSet ExecuteRaw(const std::string& sql, const std::vector<Parameter>& params = {});
    
    // Transaction control
    Transaction BeginTransaction();
    void Commit();
    void Rollback();
    
    // Connection configuration
    void SetTimeout(std::chrono::seconds timeout);
    void SetOption(ConnectionOption option, const std::any& value);
};
```

#### Connection Pooling
```cpp
// Optional connection pooling
class ConnectionPool {
public:
    ConnectionPool(const ConnectionString& connStr, size_t minPoolSize, size_t maxPoolSize);
    
    // Get a connection from the pool
    PooledConnection Acquire();
    
    // Connection pool stats
    size_t GetActiveConnectionCount() const;
    size_t GetIdleConnectionCount() const;
    
    // Pool management
    void SetMaxPoolSize(size_t size);
    void CloseIdleConnections();
};

// RAII wrapper for a pooled connection
class PooledConnection {
public:
    // Pooled connection behaves like a regular connection
    template<typename QueryType>
    ResultSet Execute(const QueryType& query);
    
    Transaction BeginTransaction();
    
    // Automatically returns to pool when destroyed
    ~PooledConnection();
};
```

### Transaction Management

#### RAII Transaction
```cpp
// RAII-based transaction management
class Transaction {
public:
    // Created via Connection::BeginTransaction()
    
    // Transaction control
    void Commit();
    void Rollback();
    
    // Auto-rollback on destruction if not committed
    ~Transaction();
    
    // Transaction options
    enum class IsolationLevel {
        ReadUncommitted,
        ReadCommitted,
        RepeatableRead,
        Serializable
    };
    
    void SetIsolationLevel(IsolationLevel level);
};

// Example usage
void TransferMoney(Connection& conn, int fromAccount, int toAccount, double amount) {
    auto txn = conn.BeginTransaction();
    try {
        // Perform operations using the same connection
        conn.Execute(Update(Accounts{}).Set(Accounts::balance, Accounts::balance - amount).Where(Accounts::id == fromAccount));
        conn.Execute(Update(Accounts{}).Set(Accounts::balance, Accounts::balance + amount).Where(Accounts::id == toAccount));
        
        txn.Commit();
    } catch (...) {
        // Transaction automatically rolled back by destructor
        throw;
    }
}
```

### Statement Execution

#### Statement Preparation
```cpp
// Internal prepared statement handling
class PreparedStatement {
private:
    // Created internally by Connection::Execute
    PreparedStatement(Connection& conn, const std::string& sql);
    
public:
    // Parameter binding
    template<typename T>
    void BindParameter(size_t index, const T& value);
    
    // Execution
    ResultSet Execute();
    
    // Statement reuse
    void Reset();
    
    // Resource cleanup
    ~PreparedStatement();
};
```

#### Parameter Binding
```cpp
// How a type-safe query gets executed
template<typename QueryType>
ResultSet Connection::Execute(const QueryType& query) {
    // Convert QueryType to SQL string and parameter list
    auto [sql, params] = query.ToSqlAndParams();
    
    // Prepare statement
    PreparedStatement stmt(*this, sql);
    
    // Bind parameters with correct types
    for (size_t i = 0; i < params.size(); ++i) {
        params[i].BindTo(stmt, i);
    }
    
    // Execute and return results
    return stmt.Execute();
}
```

### Type-Safe Result Processing

#### ResultSet Interface
```cpp
class ResultSet {
public:
    // Result set metadata
    size_t GetColumnCount() const;
    std::string GetColumnName(size_t index) const;
    ColumnType GetColumnType(size_t index) const;
    
    // Row iteration
    bool Next();
    bool HasRows() const;
    size_t GetRowCount() const;
    
    // Type-safe value access
    template<size_t Index>
    auto Get() const;
    
    template<auto MemberPtr>
    auto Get() const;
    
    // Optional value handling for NULLs
    template<auto MemberPtr>
    std::optional<std::remove_reference_t<decltype(std::declval<typename MemberPtrTraits<decltype(MemberPtr)>::ClassType>().*MemberPtr)>> 
    GetOptional() const;
    
    // Transformation helpers
    template<typename T, typename FieldMapper>
    std::vector<T> Transform(const FieldMapper& mapper) const;
    
    // C++17 structured binding support
    template<typename... Types>
    ResultView<Types...> As() const;
};
```

### Database-Specific Adaptors

#### SQLite Adaptor
```cpp
class SQLiteConnection : public ConnectionImpl {
private:
    sqlite3* db_;
    
public:
    SQLiteConnection(const std::string& filename);
    
    ResultSet ExecuteImpl(const std::string& sql, const std::vector<Parameter>& params) override;
    void BeginTransactionImpl() override;
    void CommitImpl() override;
    void RollbackImpl() override;
};
```

#### MySQL Adaptor
```cpp
class MySQLConnection : public ConnectionImpl {
private:
    MYSQL* conn_;
    
public:
    MySQLConnection(const ConnectionParams& params);
    
    ResultSet ExecuteImpl(const std::string& sql, const std::vector<Parameter>& params) override;
    void BeginTransactionImpl() override;
    void CommitImpl() override;
    void RollbackImpl() override;
};
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
// Create a connection
auto conn = Connection::CreateSQLite("database.db");

// Define schema
struct User : Table<"users"> {
    Column<"id", int> id;
    Column<"name", std::string> name;
    
    PrimaryKey<&User::id> pk;
};

// Execute a query
User u;
auto results = conn.Execute(Select(u.id, u.name).From(u).Where(u.id > 10));

// Process results
for (const auto& row : results) {
    int id = row.Get<&User::id>();
    std::string name = row.Get<&User::name>();
    std::cout << id << ": " << name << std::endl;
}
```

### Transaction with Error Handling
```cpp
Connection conn = Connection::CreateMySQL({
    .host = "localhost",
    .user = "username",
    .password = "password",
    .database = "mydb"
});

try {
    auto txn = conn.BeginTransaction();
    txn.SetIsolationLevel(Transaction::IsolationLevel::Serializable);
    
    // Execute multiple queries in the transaction
    conn.Execute(/* ... */);
    conn.Execute(/* ... */);
    
    txn.Commit();
} catch (const DatabaseException& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
    // Transaction automatically rolled back
}
```

### Connection Pooling
```cpp
// Create a connection pool
ConnectionPool pool("mysql://username:password@localhost/mydb", 5, 20);

// Function that needs a database connection
void ProcessUser(int userId) {
    // Get connection from pool
    auto conn = pool.Acquire();
    
    // Use the connection
    User u;
    auto results = conn.Execute(Select(u.name).From(u).Where(u.id == userId));
    
    // Connection automatically returned to pool when conn goes out of scope
}
```

## Error Handling Strategy

### Exception Hierarchy
```cpp
class DatabaseException : public std::runtime_error {
public:
    DatabaseException(const std::string& message);
};

class ConnectionException : public DatabaseException {
public:
    ConnectionException(const std::string& message);
};

class QueryException : public DatabaseException {
public:
    QueryException(const std::string& message, const std::string& query);
    const std::string& GetQuery() const;
};

class TransactionException : public DatabaseException {
public:
    TransactionException(const std::string& message);
};
```

### Error Information
```cpp
struct DatabaseError {
    int code;
    std::string message;
    std::string sqlState;  // For SQL standard error codes
};

DatabaseError Connection::GetLastError() const;
```
