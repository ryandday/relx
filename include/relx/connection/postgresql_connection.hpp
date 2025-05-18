#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <sstream>
#include <type_traits>

#include "connection.hpp"

// Forward declarations to avoid including libpq headers in our public API
struct pg_conn;
typedef struct pg_conn PGconn;
struct pg_result;
typedef struct pg_result PGresult;

// Forward declare statement class
namespace relx {
namespace connection {
class PostgreSQLStatement;
} // namespace connection
} // namespace relx

namespace relx {
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
        
    /// @brief Execute a raw SQL query with binary parameters
    /// @param sql The SQL query string
    /// @param params Vector of parameter values
    /// @param is_binary Vector of flags indicating whether each parameter is binary
    /// @return Result containing the query results or an error
    // TODO make binary type for bytea type in postgresql instead of this hacky function
    ConnectionResult<result::ResultSet> execute_raw_binary(
        const std::string& sql,
        const std::vector<std::string>& params,
        const std::vector<bool>& is_binary); // TODO replace vector bool
        
    /// @brief Execute a raw SQL query with typed parameters
    /// @tparam Args The types of the parameters
    /// @param sql The SQL query string
    /// @param args The parameter values
    /// @return Result containing the query results or an error
    template <typename... Args>
    ConnectionResult<result::ResultSet> execute_typed(const std::string& sql, Args&&... args) {
        // Convert each parameter to its string representation
        std::vector<std::string> param_strings;
        param_strings.reserve(sizeof...(Args));
        
        // Helper to convert a parameter to string and add to vector
        auto add_param = [&param_strings](auto&& param) {
            using ParamType = std::remove_cvref_t<decltype(param)>;
            
            if constexpr (std::is_same_v<ParamType, std::nullptr_t>) {
                // Handle NULL values
                param_strings.push_back("NULL");
            } else if constexpr (std::is_same_v<ParamType, std::string> || 
                                std::is_same_v<ParamType, const char*> ||
                                std::is_same_v<ParamType, std::string_view>) {
                // String types
                param_strings.push_back(std::string(param));
            } else if constexpr (std::is_arithmetic_v<ParamType>) {
                // Numeric types
                param_strings.push_back(std::to_string(param));
            } else if constexpr (std::is_same_v<ParamType, bool>) {
                // Boolean values
                param_strings.push_back(param ? "t" : "f");
            } else {
                // Other types, try to use stream conversion
                std::ostringstream ss;
                ss << param;
                param_strings.push_back(ss.str());
            }
        };
        
        // Add each parameter to the vector
        (add_param(std::forward<Args>(args)), ...);
        
        // Call the string-based execute_raw with our converted parameters
        return execute_raw(sql, param_strings);
    }

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

    /// @brief Create a prepared statement
    /// @param name The name of the prepared statement
    /// @param sql The SQL query text
    /// @param param_count The number of parameters in the statement
    /// @return A new prepared statement
    std::unique_ptr<PostgreSQLStatement> prepare_statement(
        const std::string& name,
        const std::string& sql,
        int param_count);
    
    /// @brief Get direct access to the PostgreSQL connection
    /// @return The PGconn pointer
    PGconn* get_pg_conn() { return pg_conn_; }

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
} // namespace relx 