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
    std::unique_ptr<boost::asio::posix::stream_descriptor> socket_;
    
    void create_socket() {
        if (conn_ == nullptr) {
            throw connection_error("Cannot create socket: no connection");
        }
        
        int sock = PQsocket(conn_);
        if (sock < 0) {
            throw connection_error("Invalid socket");
        }
        
        socket_ = std::make_unique<boost::asio::posix::stream_descriptor>(io_, sock);
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
    
    boost::asio::posix::stream_descriptor& socket() {
        if (!socket_) {
            throw connection_error("Socket not initialized");
        }
        return *socket_;
    }
    
    // Forward declare the pg_connect_state class
    class pg_connect_state;
    
    // Asynchronous connection using co_await
    class connect_awaiter {
    private:
        connection& conn_;
        std::string conninfo_;
        bool poll_write_ = false;
        bool poll_read_ = false;
        bool complete_ = false;
        std::shared_ptr<pg_connect_state> state_;
        
    public:
        connect_awaiter(connection& conn, std::string conninfo)
            : conn_(conn), conninfo_(std::move(conninfo)) {}
        
        bool await_ready() {
            if (conn_.conn_ != nullptr) {
                conn_.close();
            }
            
            // Start the connection process
            conn_.conn_ = PQconnectStart(conninfo_.c_str());
            if (conn_.conn_ == nullptr) {
                throw connection_error("Out of memory");
            }
            
            // Check if connection immediately failed
            if (PQstatus(conn_.conn_) == CONNECTION_BAD) {
                throw connection_error(conn_.conn_);
            }
            
            // Set the connection to non-blocking
            if (PQsetnonblocking(conn_.conn_, 1) != 0) {
                throw connection_error(conn_.conn_);
            }
            
            // Create the socket
            conn_.create_socket();
            
            // Check if we need to poll
            PostgresPollingStatusType poll_status = PQconnectPoll(conn_.conn_);
            
            if (poll_status == PGRES_POLLING_FAILED) {
                throw connection_error(conn_.conn_);
            }
            
            if (poll_status == PGRES_POLLING_OK) {
                complete_ = true;
                return true; // Connection immediately successful
            }
            
            // Need to poll
            poll_write_ = (poll_status == PGRES_POLLING_WRITING);
            poll_read_ = (poll_status == PGRES_POLLING_READING);
            
            return false; // Not ready, need to await
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Create a state object that will manage the lifetime of the operation
            state_ = std::make_shared<pg_connect_state>(conn_, handle, *this);
            
            // Start the connection process
            state_->start_connecting();
        }
        
        void await_resume() {
            if (!complete_) {
                throw connection_error("Connection failed");
            }
            
            if (PQstatus(conn_.conn_) != CONNECTION_OK) {
                throw connection_error(conn_.conn_);
            }
        }
        
        // Methods to be called by the state object
        void set_complete(bool complete) {
            complete_ = complete;
        }
        
        bool is_poll_read() const {
            return poll_read_;
        }
        
        bool is_poll_write() const {
            return poll_write_;
        }
        
        void set_poll_status(bool read, bool write) {
            poll_read_ = read;
            poll_write_ = write;
        }
    };
    
    // State object for asynchronous PostgreSQL connection operations
    class pg_connect_state : public std::enable_shared_from_this<pg_connect_state> {
    private:
        connection& conn_;
        std::coroutine_handle<> handle_;
        connect_awaiter& awaiter_;
        
    public:
        pg_connect_state(connection& conn, std::coroutine_handle<> handle, connect_awaiter& awaiter)
            : conn_(conn), handle_(handle), awaiter_(awaiter) {}
        
        void start_connecting() {
            if (awaiter_.is_poll_read()) {
                conn_.socket().async_wait(
                    boost::asio::posix::stream_descriptor::wait_read,
                    [self = shared_from_this()](const boost::system::error_code& ec) {
                        self->on_socket_ready(ec);
                    }
                );
            } else if (awaiter_.is_poll_write()) {
                conn_.socket().async_wait(
                    boost::asio::posix::stream_descriptor::wait_write,
                    [self = shared_from_this()](const boost::system::error_code& ec) {
                        self->on_socket_ready(ec);
                    }
                );
            }
        }
        
        void on_socket_ready(const boost::system::error_code& ec) {
            try {
                if (ec) {
                    throw boost::system::system_error(ec);
                }
                
                PostgresPollingStatusType poll_status = PQconnectPoll(conn_.conn_);
                
                if (poll_status == PGRES_POLLING_FAILED) {
                    throw connection_error(conn_.conn_);
                }
                
                if (poll_status == PGRES_POLLING_OK) {
                    awaiter_.set_complete(true);
                    handle_.resume();
                    return;
                }
                
                // If we need more polling, re-register for the appropriate event
                bool poll_read = (poll_status == PGRES_POLLING_READING);
                bool poll_write = (poll_status == PGRES_POLLING_WRITING);
                awaiter_.set_poll_status(poll_read, poll_write);
                
                if (poll_read) {
                    conn_.socket().async_wait(
                        boost::asio::posix::stream_descriptor::wait_read,
                        [self = shared_from_this()](const boost::system::error_code& ec) {
                            self->on_socket_ready(ec);
                        }
                    );
                } else if (poll_write) {
                    conn_.socket().async_wait(
                        boost::asio::posix::stream_descriptor::wait_write,
                        [self = shared_from_this()](const boost::system::error_code& ec) {
                            self->on_socket_ready(ec);
                        }
                    );
                }
            }
            catch (const std::exception& e) {
                // Set complete to false to indicate failure
                awaiter_.set_complete(false);
                
                // Resume the coroutine, which will throw in await_resume
                handle_.resume();
            }
        }
    };
    
    // Forward declare the pg_query_state class
    class pg_query_state;
    
    // Query awaiter for executing SQL queries
    class query_awaiter {
    private:
        connection& conn_;
        std::string query_;
        result result_;
        bool query_sent_ = false;
        std::shared_ptr<pg_query_state> state_;
        
    public:
        query_awaiter(connection& conn, std::string query)
            : conn_(conn), query_(std::move(query)) {}
        
        bool await_ready() {
            if (!conn_.is_open()) {
                throw connection_error("Connection is not open");
            }
            
            // Send the query
            if (!PQsendQuery(conn_.conn_, query_.c_str())) {
                throw query_error(conn_.conn_);
            }
            
            query_sent_ = true;
            
            // Check if we can consume input without blocking
            if (PQconsumeInput(conn_.conn_) == 0) {
                throw query_error(conn_.conn_);
            }
            
            // Check if we can get a result without blocking
            if (!PQisBusy(conn_.conn_)) {
                PGresult* res = PQgetResult(conn_.conn_);
                result_ = result(res);
                
                // Flush all remaining results (normally there should be none for a single query)
                while ((res = PQgetResult(conn_.conn_)) != nullptr) {
                    PQclear(res);
                }
                
                return true; // Query completed synchronously
            }
            
            return false; // Need to await
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Create a state object that will manage the lifetime of the operation
            state_ = std::make_shared<pg_query_state>(conn_, handle, *this);
            
            // Start waiting for the query result
            state_->start_waiting();
        }
        
        result await_resume() {
            if (!result_) {
                throw query_error("Query failed");
            }
            
            return std::move(result_);
        }
        
        // Set the result from state
        void set_result(result&& res) {
            result_ = std::move(res);
        }
    };
    
    // State object for asynchronous PostgreSQL operations
    class pg_query_state : public std::enable_shared_from_this<pg_query_state> {
    private:
        connection& conn_;
        std::coroutine_handle<> handle_;
        query_awaiter& awaiter_;
        
    public:
        pg_query_state(connection& conn, std::coroutine_handle<> handle, query_awaiter& awaiter)
            : conn_(conn), handle_(handle), awaiter_(awaiter) {}
        
        void start_waiting() {
            wait_for_result();
        }
        
        void wait_for_result() {
            conn_.socket().async_wait(
                boost::asio::posix::stream_descriptor::wait_read,
                [self = shared_from_this()](const boost::system::error_code& ec) {
                    self->on_socket_ready(ec);
                }
            );
        }
        
        void on_socket_ready(const boost::system::error_code& ec) {
            try {
                if (ec) {
                    throw boost::system::system_error(ec);
                }
                
                // Process available input
                if (PQconsumeInput(conn_.conn_) == 0) {
                    throw query_error(conn_.conn_);
                }
                
                // Check if we can get a result
                if (!PQisBusy(conn_.conn_)) {
                    PGresult* res = PQgetResult(conn_.conn_);
                    result result_obj(res);
                    
                    // Flush all remaining results
                    while ((res = PQgetResult(conn_.conn_)) != nullptr) {
                        PQclear(res);
                    }
                    
                    // Set the result in the awaiter
                    awaiter_.set_result(std::move(result_obj));
                    
                    // Resume the coroutine
                    handle_.resume();
                } else {
                    // Still busy, continue waiting
                    wait_for_result();
                }
            }
            catch (const std::exception& e) {
                // Set an empty result to indicate error
                awaiter_.set_result(result());
                
                // Resume the coroutine, which will throw an exception in await_resume
                handle_.resume();
            }
        }
    };
    
    // Forward declare the pg_param_query_state class
    class pg_param_query_state;
    
    // Parameterized query awaiter
    class param_query_awaiter {
    private:
        connection& conn_;
        std::string query_;
        std::vector<std::string> param_values_;
        result result_;
        bool query_sent_ = false;
        std::shared_ptr<pg_param_query_state> state_;
        
        // Convert vector of std::string to const char* array for libpq
        std::vector<const char*> get_param_values() const {
            std::vector<const char*> values;
            values.reserve(param_values_.size());
            for (const auto& param : param_values_) {
                values.push_back(param.c_str());
            }
            return values;
        }
        
    public:
        param_query_awaiter(connection& conn, std::string query, std::vector<std::string> param_values)
            : conn_(conn), query_(std::move(query)), param_values_(std::move(param_values)) {}
        
        bool await_ready() {
            if (!conn_.is_open()) {
                throw connection_error("Connection is not open");
            }
            
            // Convert parameters
            auto values = get_param_values();
            
            // Send the parameterized query
            if (!PQsendQueryParams(
                    conn_.conn_,
                    query_.c_str(),
                    static_cast<int>(values.size()),
                    nullptr, // param types - inferred
                    values.data(),
                    nullptr, // param lengths - null-terminated strings
                    nullptr, // param formats - text format
                    0 // result format - text format
                )) {
                throw query_error(conn_.conn_);
            }
            
            query_sent_ = true;
            
            // Check if we can consume input without blocking
            if (PQconsumeInput(conn_.conn_) == 0) {
                throw query_error(conn_.conn_);
            }
            
            // Check if we can get a result without blocking
            if (!PQisBusy(conn_.conn_)) {
                PGresult* res = PQgetResult(conn_.conn_);
                result_ = result(res);
                
                // Flush all remaining results (normally there should be none for a single query)
                while ((res = PQgetResult(conn_.conn_)) != nullptr) {
                    PQclear(res);
                }
                
                return true; // Query completed synchronously
            }
            
            return false; // Need to await
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Create a state object that will manage the lifetime of the operation
            state_ = std::make_shared<pg_param_query_state>(conn_, handle, *this);
            
            // Start waiting for the query result
            state_->start_waiting();
        }
        
        result await_resume() {
            if (!result_) {
                throw query_error("Query failed");
            }
            
            return std::move(result_);
        }
        
        // Set the result from state
        void set_result(result&& res) {
            result_ = std::move(res);
        }
    };
    
    // State object for asynchronous PostgreSQL parameterized operations
    class pg_param_query_state : public std::enable_shared_from_this<pg_param_query_state> {
    private:
        connection& conn_;
        std::coroutine_handle<> handle_;
        param_query_awaiter& awaiter_;
        
    public:
        pg_param_query_state(connection& conn, std::coroutine_handle<> handle, param_query_awaiter& awaiter)
            : conn_(conn), handle_(handle), awaiter_(awaiter) {}
        
        void start_waiting() {
            wait_for_result();
        }
        
        void wait_for_result() {
            conn_.socket().async_wait(
                boost::asio::posix::stream_descriptor::wait_read,
                [self = shared_from_this()](const boost::system::error_code& ec) {
                    self->on_socket_ready(ec);
                }
            );
        }
        
        void on_socket_ready(const boost::system::error_code& ec) {
            try {
                if (ec) {
                    throw boost::system::system_error(ec);
                }
                
                // Process available input
                if (PQconsumeInput(conn_.conn_) == 0) {
                    throw query_error(conn_.conn_);
                }
                
                // Check if we can get a result
                if (!PQisBusy(conn_.conn_)) {
                    PGresult* res = PQgetResult(conn_.conn_);
                    result result_obj(res);
                    
                    // Flush all remaining results
                    while ((res = PQgetResult(conn_.conn_)) != nullptr) {
                        PQclear(res);
                    }
                    
                    // Set the result in the awaiter
                    awaiter_.set_result(std::move(result_obj));
                    
                    // Resume the coroutine
                    handle_.resume();
                } else {
                    // Still busy, continue waiting
                    wait_for_result();
                }
            }
            catch (const std::exception& e) {
                // Set an empty result to indicate error
                awaiter_.set_result(result());
                
                // Resume the coroutine, which will throw an exception in await_resume
                handle_.resume();
            }
        }
    };
    
    // Co_await-able connect method
    auto connect(const std::string& conninfo) {
        return connect_awaiter(*this, conninfo);
    }
    
    // Co_await-able query method
    auto query(const std::string& query_text) {
        return query_awaiter(*this, query_text);
    }
    
    // Co_await-able parameterized query method
    auto query_params(const std::string& query_text, const std::vector<std::string>& params) {
        return param_query_awaiter(*this, query_text, params);
    }
};

} // namespace relx::pgsql_async_wrapper