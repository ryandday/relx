#include "connection/sqlite_connection.hpp"

#include <sqlite3.h>
#include <stdexcept>

namespace sqllib {
namespace connection {

SQLiteConnection::SQLiteConnection(std::string_view db_path)
    : db_path_(db_path) {}

SQLiteConnection::~SQLiteConnection() {
    disconnect();
}

SQLiteConnection::SQLiteConnection(SQLiteConnection&& other) noexcept
    : db_path_(std::move(other.db_path_)),
      db_handle_(other.db_handle_),
      is_connected_(other.is_connected_) {
    other.db_handle_ = nullptr;
    other.is_connected_ = false;
}

SQLiteConnection& SQLiteConnection::operator=(SQLiteConnection&& other) noexcept {
    if (this != &other) {
        disconnect();
        db_path_ = std::move(other.db_path_);
        db_handle_ = other.db_handle_;
        is_connected_ = other.is_connected_;
        other.db_handle_ = nullptr;
        other.is_connected_ = false;
    }
    return *this;
}

ConnectionResult<void> SQLiteConnection::connect() {
    if (is_connected_) {
        return {};  // Already connected
    }

    int result = sqlite3_open(db_path_.c_str(), &db_handle_);
    if (result != SQLITE_OK) {
        std::string error_msg = sqlite3_errmsg(db_handle_);
        sqlite3_close(db_handle_);
        db_handle_ = nullptr;
        return std::unexpected(ConnectionError{
            .message = "Failed to open SQLite database: " + error_msg,
            .error_code = result
        });
    }

    is_connected_ = true;
    return {};
}

ConnectionResult<void> SQLiteConnection::disconnect() {
    if (!is_connected_ || !db_handle_) {
        is_connected_ = false;
        db_handle_ = nullptr;
        return {};  // Already disconnected
    }

    int result = sqlite3_close(db_handle_);
    if (result != SQLITE_OK) {
        return std::unexpected(ConnectionError{
            .message = "Failed to close SQLite database: " + std::string(sqlite3_errmsg(db_handle_)),
            .error_code = result
        });
    }

    is_connected_ = false;
    db_handle_ = nullptr;
    return {};
}

ConnectionResult<result::ResultSet> SQLiteConnection::execute_raw(
    const std::string& sql, 
    const std::vector<std::string>& params) {
    
    if (!is_connected_ || !db_handle_) {
        return std::unexpected(ConnectionError{
            .message = "Not connected to database",
            .error_code = -1
        });
    }

    // Prepare the statement
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(db_handle_, sql.c_str(), -1, &stmt, nullptr);
    
    if (result != SQLITE_OK) {
        return std::unexpected(ConnectionError{
            .message = "Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(db_handle_)),
            .error_code = result
        });
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); i++) {
        result = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (result != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return std::unexpected(ConnectionError{
                .message = "Failed to bind parameter " + std::to_string(i + 1) + ": " + 
                           std::string(sqlite3_errmsg(db_handle_)),
                .error_code = result
            });
        }
    }

    // Create result set
    std::vector<std::string> column_names;
    std::vector<result::Row> rows;

    // Get column names
    int column_count = sqlite3_column_count(stmt);
    column_names.reserve(column_count);
    for (int i = 0; i < column_count; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        column_names.push_back(name ? name : "");
    }

    // Process rows
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<result::Cell> cells;
        cells.reserve(column_count);
        
        for (int i = 0; i < column_count; i++) {
            if (sqlite3_column_type(stmt, i) == SQLITE_NULL) {
                cells.emplace_back("NULL");
            } else {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                cells.emplace_back(text ? text : "");
            }
        }
        
        rows.push_back(result::Row(std::move(cells), column_names));
    }

    if (result != SQLITE_DONE) {
        std::string error_msg = sqlite3_errmsg(db_handle_);
        sqlite3_finalize(stmt);
        return std::unexpected(ConnectionError{
            .message = "Error executing SQLite query: " + error_msg,
            .error_code = result
        });
    }

    sqlite3_finalize(stmt);
    
    // Create the result set from the data we collected
    return result::ResultSet(std::move(rows), std::move(column_names));
}

bool SQLiteConnection::is_connected() const {
    return is_connected_ && db_handle_ != nullptr;
}

} // namespace connection
} // namespace sqllib 