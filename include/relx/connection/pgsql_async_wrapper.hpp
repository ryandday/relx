// Asynchronous PostgreSQL Client with C++20 Coroutines
// This library wraps libpq's asynchronous API with C++20 coroutines and Boost.Asio
// to provide a clean, modern interface for PostgreSQL database operations.

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include <libpq-fe.h>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <memory>
#include <exception>
#include <stdexcept>
#include <utility>
#include <functional>
#include <iostream>
#include <chrono>

namespace relx::pgsql_async_wrapper {

// Exception classes
class pg_error : public std::runtime_error {
public:
    explicit pg_error(const std::string& msg) : std::runtime_error(msg) {}
    explicit pg_error(const char* msg) : std::runtime_error(msg) {}
};

class connection_error : public pg_error {
public:
    explicit connection_error(const std::string& msg) : pg_error(msg) {}
    explicit connection_error(const char* msg) : pg_error(msg) {}
    explicit connection_error(PGconn* conn) 
        : pg_error(PQerrorMessage(conn) ? PQerrorMessage(conn) : "Unknown PostgreSQL connection error") {}
};

class query_error : public pg_error {
public:
    explicit query_error(const std::string& msg) : pg_error(msg) {}
    explicit query_error(const char* msg) : pg_error(msg) {}
    explicit query_error(PGresult* result) 
        : pg_error(PQresultErrorMessage(result) ? PQresultErrorMessage(result) : "Unknown PostgreSQL query error") {}
    explicit query_error(PGconn* conn) 
        : pg_error(PQerrorMessage(conn) ? PQerrorMessage(conn) : "Unknown PostgreSQL query error") {}
};

// Result class to handle PGresult
class result {
private:
    PGresult* res_ = nullptr;
    
public:
    result() = default;
    
    explicit result(PGresult* res) : res_(res) {}
    
    ~result() {
        clear();
    }
    
    result(const result&) = delete;
    result& operator=(const result&) = delete;
    
    result(result&& other) noexcept : res_(other.res_) {
        other.res_ = nullptr;
    }
    
    result& operator=(result&& other) noexcept {
        if (this != &other) {
            clear();
            res_ = other.res_;
            other.res_ = nullptr;
        }
        return *this;
    }
    
    void clear() {
        if (res_) {
            PQclear(res_);
            res_ = nullptr;
        }
    }
    
    bool ok() const {
        if (!res_) return false;
        auto status = PQresultStatus(res_);
        return status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
    }
    
    ExecStatusType status() const {
        return res_ ? PQresultStatus(res_) : PGRES_FATAL_ERROR;
    }
    
    const char* error_message() const {
        return res_ ? PQresultErrorMessage(res_) : "No result available";
    }
    
    int rows() const {
        return res_ ? PQntuples(res_) : 0;
    }
    
    int columns() const {
        return res_ ? PQnfields(res_) : 0;
    }
    
    const char* field_name(int col) const {
        return res_ ? PQfname(res_, col) : nullptr;
    }
    
    Oid field_type(int col) const {
        return res_ ? PQftype(res_, col) : 0;
    }
    
    int field_size(int col) const {
        return res_ ? PQfsize(res_, col) : 0;
    }
    
    int field_number(const char* name) const {
        return res_ ? PQfnumber(res_, name) : -1;
    }
    
    bool is_null(int row, int col) const {
        return res_ ? PQgetisnull(res_, row, col) != 0 : true;
    }
    
    const char* get_value(int row, int col) const {
        return res_ ? PQgetvalue(res_, row, col) : nullptr;
    }
    
    int get_length(int row, int col) const {
        return res_ ? PQgetlength(res_, row, col) : 0;
    }
    
    PGresult* get() const {
        return res_;
    }
    
    operator bool() const {
        return ok();
    }
};

// Forward declarations
class connection;

// ----------------------------------------------------------------------
// The connection class - main interface for PostgreSQL operations
// ----------------------------------------------------------------------
class connection {
private:
    boost::asio::io_context& io_;
    PGconn* conn_ = nullptr;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    
    void create_socket() {
        if (conn_ == nullptr) {
            throw connection_error("Cannot create socket: no connection");
        }
        
        int sock = PQsocket(conn_);
        if (sock < 0) {
            throw connection_error("Invalid socket");
        }
        
        socket_ = std::make_unique<boost::asio::ip::tcp::socket>(io_, boost::asio::ip::tcp::v4(), sock);
    }
    
public:
    connection(boost::asio::io_context& io) : io_(io) {}
    
    ~connection() {
        close();
    }
    
    // Non-copyable
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
    
    // Move constructible/assignable
    connection(connection&& other) noexcept
        : io_(other.io_), conn_(other.conn_), socket_(std::move(other.socket_)) {
        other.conn_ = nullptr;
    }
    
    connection& operator=(connection&& other) noexcept {
        if (this != &other) {
            close();
            conn_ = other.conn_;
            socket_ = std::move(other.socket_);
            other.conn_ = nullptr;
        }
        return *this;
    }
    
    void close() {
        if (socket_) {
            socket_->close();
            socket_.reset();
        }
        
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }
    
    bool is_open() const {
        return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
    }
    
    PGconn* native_handle() {
        return conn_;
    }
    
    boost::asio::ip::tcp::socket& socket() {
        if (!socket_) {
            throw connection_error("Socket not initialized");
        }
        return *socket_;
    }
    
    // Asynchronous connection using boost::asio::awaitable
    boost::asio::awaitable<void> connect(const std::string& conninfo) {
        if (conn_ != nullptr) {
            close();
        }
        
        // Start the connection process
        conn_ = PQconnectStart(conninfo.c_str());
        if (conn_ == nullptr) {
            throw connection_error("Out of memory");
        }
        
        // Check if connection immediately failed
        if (PQstatus(conn_) == CONNECTION_BAD) {
            throw connection_error(conn_);
        }
        
        // Set the connection to non-blocking
        if (PQsetnonblocking(conn_, 1) != 0) {
            throw connection_error(conn_);
        }
        
        // Create the socket
        create_socket();
        
        // Connection polling loop
        while (true) {
            PostgresPollingStatusType poll_status = PQconnectPoll(conn_);
            
            if (poll_status == PGRES_POLLING_FAILED) {
                throw connection_error(conn_);
            }
            
            if (poll_status == PGRES_POLLING_OK) {
                // Connection successful
                break;
            }
            
            // Need to poll
            if (poll_status == PGRES_POLLING_READING) {
                // Wait until socket is readable
                boost::system::error_code ec;
                co_await socket().async_wait(
                    boost::asio::ip::tcp::socket::wait_read, 
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                );
                
                if (ec) {
                    throw boost::system::system_error(ec);
                }
            }
            else if (poll_status == PGRES_POLLING_WRITING) {
                // Wait until socket is writable
                boost::system::error_code ec;
                co_await socket().async_wait(
                    boost::asio::ip::tcp::socket::wait_write, 
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                );
                
                if (ec) {
                    throw boost::system::system_error(ec);
                }
            }
        }
        
        if (PQstatus(conn_) != CONNECTION_OK) {
            throw connection_error(conn_);
        }
        
        co_return;
    }
    
    // Asynchronous query execution using boost::asio::awaitable
    boost::asio::awaitable<result> query(const std::string& query_text) {
        if (!is_open()) {
            throw connection_error("Connection is not open");
        }
        
        // Send the query
        if (!PQsendQuery(conn_, query_text.c_str())) {
            throw query_error(conn_);
        }
        
        // Flush the outgoing data
        while (true) {
            int flush_result = PQflush(conn_);
            if (flush_result == -1) {
                throw query_error(conn_);
            }
            if (flush_result == 0) {
                break; // All data has been flushed
            }
            // Flush returned 1, need to wait for socket to be writable
            boost::system::error_code ec;
            co_await socket().async_wait(
                boost::asio::ip::tcp::socket::wait_write,
                boost::asio::redirect_error(boost::asio::use_awaitable, ec)
            );
            
            if (ec) {
                throw boost::system::system_error(ec);
            }
        }
        
        // Process input and wait for results
        while (true) {
            // Check if we can consume input without blocking
            if (PQconsumeInput(conn_) == 0) {
                throw query_error(conn_);
            }
            
            // Check if we can get a result without blocking
            if (!PQisBusy(conn_)) {
                PGresult* res = PQgetResult(conn_);
                result result_obj(res);
                
                // Clear all remaining results (normally there should be none for a single query)
                while ((res = PQgetResult(conn_)) != nullptr) {
                    PQclear(res);
                }
                
                co_return result_obj;
            }
            
            // Still busy, wait for the socket to be readable
            boost::system::error_code ec;
            co_await socket().async_wait(
                boost::asio::ip::tcp::socket::wait_read,
                boost::asio::redirect_error(boost::asio::use_awaitable, ec)
            );
            
            if (ec) {
                throw boost::system::system_error(ec);
            }
        }
    }
    
    // Asynchronous parameterized query execution using boost::asio::awaitable
    boost::asio::awaitable<result> query_params(const std::string& query_text, const std::vector<std::string>& params) {
        if (!is_open()) {
            throw connection_error("Connection is not open");
        }
        
        // Convert parameters
        std::vector<const char*> values;
        values.reserve(params.size());
        for (const auto& param : params) {
            values.push_back(param.c_str());
        }
        
        // Send the parameterized query
        if (!PQsendQueryParams(
                conn_,
                query_text.c_str(),
                static_cast<int>(values.size()),
                nullptr, // param types - inferred
                values.data(),
                nullptr, // param lengths - null-terminated strings
                nullptr, // param formats - text format
                0 // result format - text format
            )) {
            throw query_error(conn_);
        }
        
        // Flush the outgoing data
        while (true) {
            int flush_result = PQflush(conn_);
            if (flush_result == -1) {
                throw query_error(conn_);
            }
            if (flush_result == 0) {
                break; // All data has been flushed
            }
            // Flush returned 1, need to wait for socket to be writable
            boost::system::error_code ec;
            co_await socket().async_wait(
                boost::asio::ip::tcp::socket::wait_write,
                boost::asio::redirect_error(boost::asio::use_awaitable, ec)
            );
            
            if (ec) {
                throw boost::system::system_error(ec);
            }
        }
        
        // Process input and wait for results
        while (true) {
            // Check if we can consume input without blocking
            if (PQconsumeInput(conn_) == 0) {
                throw query_error(conn_);
            }
            
            // Check if we can get a result without blocking
            if (!PQisBusy(conn_)) {
                PGresult* res = PQgetResult(conn_);
                result result_obj(res);
                
                // Flush all remaining results (normally there should be none for a single query)
                while ((res = PQgetResult(conn_)) != nullptr) {
                    PQclear(res);
                }
                
                co_return result_obj;
            }
            
            // Still busy, wait for the socket to be readable
            boost::system::error_code ec;
            co_await socket().async_wait(
                boost::asio::ip::tcp::socket::wait_read,
                boost::asio::redirect_error(boost::asio::use_awaitable, ec)
            );
            
            if (ec) {
                throw boost::system::system_error(ec);
            }
        }
    }
};

} // namespace relx::pgsql_async_wrapper