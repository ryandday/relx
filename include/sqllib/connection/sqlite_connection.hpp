#pragma once

#include <string>
#include <string_view>
#include <memory>

#include "connection.hpp"

// Forward declaration to avoid including SQLite header in our public API
struct sqlite3;
struct sqlite3_stmt;

namespace sqllib {
namespace connection {

/// @brief SQLite implementation of the Connection interface
class SQLiteConnection : public Connection {
public:
    /// @brief Constructor with database path
    /// @param db_path Path to the SQLite database file
    explicit SQLiteConnection(std::string_view db_path);
    
    /// @brief Destructor that ensures proper cleanup
    ~SQLiteConnection() override;
    
    // Delete copy constructor and assignment operator
    SQLiteConnection(const SQLiteConnection&) = delete;
    SQLiteConnection& operator=(const SQLiteConnection&) = delete;
    
    // Allow move operations
    SQLiteConnection(SQLiteConnection&&) noexcept;
    SQLiteConnection& operator=(SQLiteConnection&&) noexcept;

    /// @brief Connect to the SQLite database
    /// @return Result indicating success or failure
    ConnectionResult<void> connect() override;

    /// @brief Disconnect from the SQLite database
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

private:
    std::string db_path_;
    sqlite3* db_handle_ = nullptr;
    bool is_connected_ = false;
};

} // namespace connection
} // namespace sqllib 