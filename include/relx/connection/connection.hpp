#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <expected>
#include <vector>

#include "../results/result.hpp"
#include "../query/core.hpp"

namespace relx {
namespace connection {

/// @brief Error type for database connection operations
struct ConnectionError {
    std::string message;
    int error_code = 0;
};

/// @brief Type alias for result of connection operations
template <typename T>
using ConnectionResult = std::expected<T, ConnectionError>;

/// @brief Transaction isolation levels
enum class IsolationLevel {
    ReadUncommitted,  ///< Allows dirty reads
    ReadCommitted,    ///< Prevents dirty reads
    RepeatableRead,   ///< Prevents non-repeatable reads
    Serializable      ///< Highest isolation level, prevents phantom reads
};

/// @brief Abstract base class for database connections
class Connection {
public:
    /// @brief Virtual destructor
    virtual ~Connection() = default;

    /// @brief Connect to the database
    /// @return Result indicating success or failure
    virtual ConnectionResult<void> connect() = 0;

    /// @brief Disconnect from the database
    /// @return Result indicating success or failure
    virtual ConnectionResult<void> disconnect() = 0;

    /// @brief Execute a raw SQL query with parameters
    /// @param sql The SQL query string
    /// @param params Vector of parameter values
    /// @return Result containing the query results or an error
    virtual ConnectionResult<result::ResultSet> execute_raw(
        const std::string& sql, 
        const std::vector<std::string>& params = {}) = 0;

    /// @brief Execute a query expression
    /// @param query The query expression to execute
    /// @return Result containing the query results or an error
    template <query::SqlExpr Query>
    ConnectionResult<result::ResultSet> execute(const Query& query) {
        std::string sql = query.to_sql();
        std::vector<std::string> params = query.bind_params();
        return execute_raw(sql, params);
    }

    /// @brief Check if the connection is open
    /// @return True if connected, false otherwise
    virtual bool is_connected() const = 0;
    
    /// @brief Begin a new transaction
    /// @param isolation_level The isolation level for the transaction
    /// @return Result indicating success or failure
    virtual ConnectionResult<void> begin_transaction(
        IsolationLevel isolation_level = IsolationLevel::ReadCommitted) = 0;
    
    /// @brief Commit the current transaction
    /// @return Result indicating success or failure
    virtual ConnectionResult<void> commit_transaction() = 0;
    
    /// @brief Rollback the current transaction
    /// @return Result indicating success or failure
    virtual ConnectionResult<void> rollback_transaction() = 0;
    
    /// @brief Check if a transaction is currently active
    /// @return True if a transaction is active, false otherwise
    virtual bool in_transaction() const = 0;
};

} // namespace connection

// Convenient imports from the connection namespace
using connection::Connection;
using connection::ConnectionError;
using connection::ConnectionResult;
using connection::IsolationLevel;

} // namespace relx