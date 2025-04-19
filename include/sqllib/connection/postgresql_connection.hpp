#pragma once

#include <string>
#include <string_view>
#include <memory>

#include "connection.hpp"

// Forward declarations to avoid including libpq headers in our public API
struct pg_conn;
typedef struct pg_conn PGconn;
struct pg_result;
typedef struct pg_result PGresult;

namespace sqllib {
namespace connection {

/// @brief PostgreSQL implementation of the Connection interface
class PostgreSQLConnection : public Connection {
public:
    /// @brief Constructor with connection parameters
    /// @param connection_string PostgreSQL connection string (e.g. "host=localhost port=5432 dbname=mydb user=postgres password=password")
    explicit PostgreSQLConnection(std::string_view connection_string);
    
    /// @brief Destructor that ensures proper cleanup
    ~PostgreSQLConnection() override;
    
    // Delete copy constructor and assignment operator
    PostgreSQLConnection(const PostgreSQLConnection&) = delete;
    PostgreSQLConnection& operator=(const PostgreSQLConnection&) = delete;
    
    // Allow move operations
    PostgreSQLConnection(PostgreSQLConnection&&) noexcept;
    PostgreSQLConnection& operator=(PostgreSQLConnection&&) noexcept;

    /// @brief Connect to the PostgreSQL database
    /// @return Result indicating success or failure
    ConnectionResult<void> connect() override;

    /// @brief Disconnect from the PostgreSQL database
    /// @return Result indicating success or failure
    ConnectionResult<void> disconnect() override;

    /// @brief Execute a raw SQL query with parameters
    /// @param sql The SQL query string
    /// @param params Vector of parameter values
    /// @return Result containing the query results or an error
    ConnectionResult<result::ResultSet> execute_raw(
        const std::string& sql, 
        const std::vector<std::string>& params = {}) override;

    /// @brief Check if the connection is open
    /// @return True if connected, false otherwise
    bool is_connected() const override;
    
    /// @brief Begin a new transaction with specified isolation level
    /// @param isolation_level The isolation level for the transaction
    /// @return Result indicating success or failure
    ConnectionResult<void> begin_transaction(
        IsolationLevel isolation_level = IsolationLevel::ReadCommitted) override;
    
    /// @brief Commit the current transaction
    /// @return Result indicating success or failure
    ConnectionResult<void> commit_transaction() override;
    
    /// @brief Rollback the current transaction
    /// @return Result indicating success or failure
    ConnectionResult<void> rollback_transaction() override;
    
    /// @brief Check if a transaction is currently active
    /// @return True if a transaction is active, false otherwise
    bool in_transaction() const override;

private:
    std::string connection_string_;
    PGconn* pg_conn_ = nullptr;
    bool is_connected_ = false;
    bool in_transaction_ = false;

    /// @brief Helper method to handle PGresult and convert to ConnectionResult
    /// @param result PGresult pointer to process
    /// @param expected_status Expected status code (or -1 to ignore)
    /// @return ConnectionResult with error or success
    ConnectionResult<PGresult*> handle_pg_result(PGresult* result, int expected_status = -1);
    
    /// @brief Convert SQL with ? placeholders to PostgreSQL's $n format
    /// @param sql SQL query with ? placeholders
    /// @return Converted SQL with $1, $2, etc. placeholders
    std::string convert_placeholders(const std::string& sql);
};

} // namespace connection
} // namespace sqllib 