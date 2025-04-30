#include "relx/connection/postgresql_async_connection.hpp"
#include <libpq-fe.h>
#include <functional>
#include <cstring>
#include <regex>

using namespace relx::connection;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;

// Constructor with io_context
PostgreSQLAsyncConnection::PostgreSQLAsyncConnection(
    boost::asio::io_context& io_context,
    std::string_view connection_string) 
    : io_context_(io_context), 
      connection_string_(connection_string) {
}

// Destructor
PostgreSQLAsyncConnection::~PostgreSQLAsyncConnection() {
    if (is_connected_) {
        PQfinish(pg_conn_);
        pg_conn_ = nullptr;
        is_connected_ = false;
    }
}

// Helper to wait for socket to be readable
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::wait_for_readable() {
    try {
        co_await socket_->async_wait(tcp::socket::wait_read, boost::asio::use_awaitable);
        co_return ConnectionResult<void>{};
    } catch (const boost::system::system_error& e) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Socket read error: ") + e.what(),
            .error_code = e.code().value()
        });
    }
}

// Helper to wait for socket to be writable
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::wait_for_writable() {
    try {
        co_await socket_->async_wait(tcp::socket::wait_write, boost::asio::use_awaitable);
        co_return ConnectionResult<void>{};
    } catch (const boost::system::system_error& e) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Socket write error: ") + e.what(),
            .error_code = e.code().value()
        });
    }
}

// Poll a PostgreSQL connection during connect
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::poll_connection() {
    while (true) {
        PostgresPollingStatusType poll_status = PQconnectPoll(pg_conn_);
        
        switch (poll_status) {
            case PGRES_POLLING_FAILED:
            {
                std::string error_msg = PQerrorMessage(pg_conn_);
                PQfinish(pg_conn_);
                pg_conn_ = nullptr;
                
                co_return std::unexpected(ConnectionError{
                    .message = "Connection polling failed: " + error_msg,
                    .error_code = -1
                });
            }
            case PGRES_POLLING_OK:
            {
                // Connection complete
                is_connected_ = true;
                co_return ConnectionResult<void>{};
            }
            case PGRES_POLLING_READING:
            {
                // Need to wait for socket to be readable
                auto wait_result = co_await wait_for_readable();
                if (!wait_result) {
                    PQfinish(pg_conn_);
                    pg_conn_ = nullptr;
                    co_return wait_result;
                }
                break; // Continue polling
            }
            case PGRES_POLLING_WRITING:
            {
                // Need to wait for socket to be writable
                auto wait_result = co_await wait_for_writable();
                if (!wait_result) {
                    PQfinish(pg_conn_);
                    pg_conn_ = nullptr;
                    co_return wait_result;
                }
                break; // Continue polling
            }
            default:
                break;
        }
    }
}

// Connect asynchronously
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::connect() {
    if (is_connected_) {
        co_return ConnectionResult<void>{}; // Already connected
    }
    
    try {
        // Start a non-blocking connection
        pg_conn_ = PQconnectStart(connection_string_.c_str());
        if (pg_conn_ == nullptr) {
            co_return std::unexpected(ConnectionError{
                .message = "Failed to start connection (memory allocation failure)",
                .error_code = -1
            });
        }
        
        if (PQstatus(pg_conn_) == CONNECTION_BAD) {
            std::string error_msg = PQerrorMessage(pg_conn_);
            PQfinish(pg_conn_);
            pg_conn_ = nullptr;
            
            co_return std::unexpected(ConnectionError{
                .message = "Connection failed: " + error_msg,
                .error_code = -1
            });
        }
        
        // Set socket to non-blocking mode
        PQsetnonblocking(pg_conn_, 1);
        
        // Get the socket descriptor
        int sock = PQsocket(pg_conn_);
        if (sock < 0) {
            std::string error_msg = PQerrorMessage(pg_conn_);
            PQfinish(pg_conn_);
            pg_conn_ = nullptr;
            
            co_return std::unexpected(ConnectionError{
                .message = "Invalid socket: " + error_msg,
                .error_code = -1
            });
        }
        
        // Create socket wrapper for asio
        // Note: We're using boost::asio::ip::tcp::socket::native_handle_type
        // to manage the socket descriptor that's already created by libpq
        socket_ = std::make_unique<tcp::socket>(io_context_, tcp::v4(), sock);
        
        // Start polling the connection
        auto result = co_await poll_connection();
        co_return result;
    } catch (const std::exception& e) {
        if (pg_conn_ != nullptr) {
            PQfinish(pg_conn_);
            pg_conn_ = nullptr;
        }
        
        co_return std::unexpected(ConnectionError{
            .message = std::string("Exception during connection: ") + e.what(),
            .error_code = -1
        });
    }
}

// Disconnect asynchronously
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::disconnect() {
    if (!is_connected_) {
        co_return ConnectionResult<void>{}; // Already disconnected
    }
    
    try {
        // Close the socket first
        if (socket_ && socket_->is_open()) {
            boost::system::error_code ec;
            socket_->close(ec);
            if (ec) {
                // Just log error, continue with PG cleanup
                // Don't return here, still need to clean up PG connection
            }
            socket_.reset();
        }
        
        // Clean up PostgreSQL connection
        if (pg_conn_ != nullptr) {
            PQfinish(pg_conn_);
            pg_conn_ = nullptr;
        }
        
        is_connected_ = false;
        in_transaction_ = false;
        
        co_return ConnectionResult<void>{};
    } catch (const std::exception& e) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Exception during disconnect: ") + e.what(),
            .error_code = -1
        });
    }
}

// Helper to process query results asynchronously
boost::asio::awaitable<ConnectionResult<relx::result::ResultSet>> PostgreSQLAsyncConnection::process_query_result() {
    try {
        std::vector<relx::result::Row> rows;
        std::vector<std::string> column_names;
        while (true) {
            // Check if PQconsumeInput can read data without blocking
            if (!PQconsumeInput(pg_conn_)) {
                std::string error_msg = PQerrorMessage(pg_conn_);
                co_return std::unexpected(ConnectionError{
                    .message = "Error consuming query input: " + error_msg,
                    .error_code = -1
                });
            }
            
            // Check if we have a result ready
            if (PQisBusy(pg_conn_)) {
                // Not ready yet, wait for more data
                auto wait_result = co_await wait_for_readable();
                if (!wait_result) {
                    co_return std::unexpected(wait_result.error());
                }
                continue;
            }
            
            // Get the result
            PGresult* pg_result = PQgetResult(pg_conn_);
            
            // If no more results, we're done
            if (pg_result == nullptr) {
                co_return relx::result::ResultSet(std::move(rows), std::move(column_names));
            }
            
            // Check for errors
            ExecStatusType status = PQresultStatus(pg_result);
            if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
                std::string error_msg = PQresultErrorMessage(pg_result);
                PQclear(pg_result);
                
                // Consume any remaining results
                while ((pg_result = PQgetResult(pg_conn_)) != nullptr) {
                    PQclear(pg_result);
                }
                
                co_return std::unexpected(ConnectionError{
                    .message = "Query execution failed: " + error_msg,
                    .error_code = status
                });
            }
            
            // If this is a command (non-SELECT), return empty result set
            if (status == PGRES_COMMAND_OK) {
                PQclear(pg_result);
                
                // Consume any remaining results
                while ((pg_result = PQgetResult(pg_conn_)) != nullptr) {
                    PQclear(pg_result);
                }
                
                co_return relx::result::ResultSet(std::move(rows), std::move(column_names));
            }
            
            // Process rows and columns for SELECT results
            int output_rows = PQntuples(pg_result);
            int output_cols = PQnfields(pg_result);
            
            // Add column names only if they haven't been added yet
            if (column_names.empty()) {
                for (int col = 0; col < output_cols; ++col) {
                    column_names.push_back(PQfname(pg_result, col));
                }
            }
            
            // Add rows
            std::vector<relx::result::Row> rows;
            for (int row = 0; row < output_rows; ++row) {
                std::vector<relx::result::Cell> cells;
                
                for (int col = 0; col < output_cols; ++col) {
                    if (PQgetisnull(pg_result, row, col)) {
                        cells.push_back(relx::result::Cell("NULL"));
                    } else {
                        char* value = PQgetvalue(pg_result, row, col);
                        int length = PQgetlength(pg_result, row, col);
                        cells.push_back(relx::result::Cell(std::string(value, length)));
                    }
                }
                
                rows.push_back(relx::result::Row(std::move(cells), std::move(column_names)));
            }
            
            PQclear(pg_result);
            
            // Check if we have more results
            if (PQsetSingleRowMode(pg_conn_)) {
                // Continue fetching results
                continue;
            } else {
                // No more results expected
                break;
            }
        }
        
        // Consume any remaining results (should not happen with single row mode)
        PGresult* pg_result;
        while ((pg_result = PQgetResult(pg_conn_)) != nullptr) {
            PQclear(pg_result);
        }
        
        co_return relx::result::ResultSet(std::move(rows), std::move(column_names));
    } catch (const std::exception& e) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Exception during query processing: ") + e.what(),
            .error_code = -1
        });
    }
}

// Execute a raw SQL query asynchronously
boost::asio::awaitable<ConnectionResult<relx::result::ResultSet>> PostgreSQLAsyncConnection::execute_raw(
    const std::string& sql, 
    const std::vector<std::string>& params) {
    if (!is_connected_) {
        co_return std::unexpected(ConnectionError{
            .message = "Not connected to the database",
            .error_code = -1
        });
    }
    
    try {
        // Convert placeholders if needed
        std::string processed_sql = convert_placeholders(sql);
        
        // Prepare parameters for async query
        int param_count = static_cast<int>(params.size());
        std::vector<const char*> param_values(param_count);
        
        for (int i = 0; i < param_count; ++i) {
            param_values[i] = params[i].c_str();
        }
        
        // Start the async query
        int send_result = PQsendQueryParams(
            pg_conn_,
            processed_sql.c_str(),
            param_count,
            nullptr,  // param types - let server infer
            param_values.data(),
            nullptr,  // param lengths - null terminated
            nullptr,  // param formats - all text
            0        // result format - text
        );
        
        if (send_result == 0) {
            std::string error_msg = PQerrorMessage(pg_conn_);
            co_return std::unexpected(ConnectionError{
                .message = "Failed to send query: " + error_msg,
                .error_code = -1
            });
        }
        
        // Process the results
        auto result = co_await process_query_result();
        co_return result;
    } catch (const std::exception& e) {
        co_return std::unexpected(ConnectionError{
            .message = std::string("Exception during query execution: ") + e.what(),
            .error_code = -1
        });
    }
}

// Begin a transaction asynchronously
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::begin_transaction(
    IsolationLevel isolation_level) {
    if (!is_connected_) {
        co_return std::unexpected(ConnectionError{
            .message = "Not connected to the database",
            .error_code = -1
        });
    }
    
    if (in_transaction_) {
        co_return ConnectionResult<void>{}; // Already in a transaction
    }
    
    std::string isolation_str;
    switch (isolation_level) {
        case IsolationLevel::ReadUncommitted:
            isolation_str = "READ UNCOMMITTED";
            break;
        case IsolationLevel::ReadCommitted:
            isolation_str = "READ COMMITTED";
            break;
        case IsolationLevel::RepeatableRead:
            isolation_str = "REPEATABLE READ";
            break;
        case IsolationLevel::Serializable:
            isolation_str = "SERIALIZABLE";
            break;
    }
    
    std::string sql = "BEGIN ISOLATION LEVEL " + isolation_str;
    auto result = co_await execute_raw(sql);
    
    if (result) {
        in_transaction_ = true;
        co_return ConnectionResult<void>{};
    } else {
        co_return std::unexpected(result.error());
    }
}

// Commit a transaction asynchronously
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::commit_transaction() {
    if (!is_connected_) {
        co_return std::unexpected(ConnectionError{
            .message = "Not connected to the database",
            .error_code = -1
        });
    }
    
    if (!in_transaction_) {
        co_return ConnectionResult<void>{}; // Not in a transaction
    }
    
    auto result = co_await execute_raw("COMMIT");
    
    if (result) {
        in_transaction_ = false;
        co_return ConnectionResult<void>{};
    } else {
        co_return std::unexpected(result.error());
    }
}

// Rollback a transaction asynchronously
boost::asio::awaitable<ConnectionResult<void>> PostgreSQLAsyncConnection::rollback_transaction() {
    if (!is_connected_) {
        co_return std::unexpected(ConnectionError{
            .message = "Not connected to the database",
            .error_code = -1
        });
    }
    
    if (!in_transaction_) {
        co_return ConnectionResult<void>{}; // Not in a transaction
    }
    
    auto result = co_await execute_raw("ROLLBACK");
    
    if (result) {
        in_transaction_ = false;
        co_return ConnectionResult<void>{};
    } else {
        co_return std::unexpected(result.error());
    }
}

// Check if connected
bool PostgreSQLAsyncConnection::is_connected() const {
    return is_connected_ && pg_conn_ != nullptr && 
           PQstatus(pg_conn_) != CONNECTION_BAD;
}

// Check if in transaction
bool PostgreSQLAsyncConnection::in_transaction() const {
    return in_transaction_;
}

// Helper to handle PostgreSQL result
ConnectionResult<PGresult*> PostgreSQLAsyncConnection::handle_pg_result(
    PGresult* result, int expected_status) {
    if (result == nullptr) {
        return std::unexpected(ConnectionError{
            .message = "NULL result from PostgreSQL",
            .error_code = -1
        });
    }
    
    ExecStatusType status = PQresultStatus(result);
    
    // If an expected status was specified, check for a match
    if (expected_status != -1 && status != expected_status) {
        std::string error_msg = PQresultErrorMessage(result);
        PQclear(result);
        return std::unexpected(ConnectionError{
            .message = error_msg,
            .error_code = status
        });
    }
    
    // Check for errors
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK && 
        status != PGRES_SINGLE_TUPLE) {
        std::string error_msg = PQresultErrorMessage(result);
        PQclear(result);
        return std::unexpected(ConnectionError{
            .message = error_msg,
            .error_code = status
        });
    }
    
    return result;
}

// Convert placeholders from ? to $1, $2, etc.
std::string PostgreSQLAsyncConnection::convert_placeholders(const std::string& sql) {
    std::regex placeholder_regex("\\?");
    int param_number = 1;
    
    std::string result;
    std::string::const_iterator search_start = sql.begin();
    std::smatch match;
    
    while (std::regex_search(search_start, sql.end(), match, placeholder_regex)) {
        result.append(match.prefix());
        result.append("$" + std::to_string(param_number++));
        search_start = match.suffix().first;
    }
    
    result.append(std::string(search_start, sql.end()));
    
    return result;
} 