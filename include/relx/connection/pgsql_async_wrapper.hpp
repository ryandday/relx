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

namespace relx::connection::pgsql_async {

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

// Forward declaration of connection class
class connection;

// State objects for handling async operations with proper lifetime management

// Connect state object
struct pg_connect_state : std::enable_shared_from_this<pg_connect_state> {
    connection& conn_;
    std::string conninfo_;
    std::coroutine_handle<> handle_;
    bool poll_write_ = false;
    bool poll_read_ = false;
    bool complete_ = false;
    
    pg_connect_state(connection& conn, std::string conninfo, std::coroutine_handle<> handle)
        : conn_(conn), conninfo_(std::move(conninfo)), handle_(handle) {}
    
    void start();
    void poll_connection();
    void on_socket_ready(const boost::system::error_code& ec);
};

// Query state object
struct pg_query_state : std::enable_shared_from_this<pg_query_state> {
    connection& conn_;
    std::string query_;
    std::coroutine_handle<> handle_;
    result result_;
    bool query_sent_ = false;
    
    pg_query_state(connection& conn, std::string query, std::coroutine_handle<> handle)
        : conn_(conn), query_(std::move(query)), handle_(handle) {}
    
    void start();
    void wait_for_result();
    void on_socket_ready(const boost::system::error_code& ec);
};

// Parameterized query state object
struct pg_param_query_state : std::enable_shared_from_this<pg_param_query_state> {
    connection& conn_;
    std::string query_;
    std::vector<std::string> param_values_;
    std::coroutine_handle<> handle_;
    result result_;
    bool query_sent_ = false;
    
    // Convert vector of std::string to const char* array for libpq
    std::vector<const char*> get_param_values() const {
        std::vector<const char*> values;
        values.reserve(param_values_.size());
        for (const auto& param : param_values_) {
            values.push_back(param.c_str());
        }
        return values;
    }
    
    pg_param_query_state(connection& conn, std::string query, std::vector<std::string> param_values, std::coroutine_handle<> handle)
        : conn_(conn), query_(std::move(query)), param_values_(std::move(param_values)), handle_(handle) {}
    
    void start();
    void wait_for_result();
    void on_socket_ready(const boost::system::error_code& ec);
};

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
    
    // Asynchronous connection using co_await
    class connect_awaiter {
    private:
        connection& conn_;
        std::string conninfo_;
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
                return true; // Connection immediately successful
            }
            
            return false; // Not ready, need to await
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Create and start the connection state object
            state_ = std::make_shared<pg_connect_state>(conn_, conninfo_, handle);
            state_->start();
        }
        
        void await_resume() {
            if (!state_->complete_) {
                throw connection_error("Connection failed");
            }
            
            if (PQstatus(conn_.conn_) != CONNECTION_OK) {
                throw connection_error(conn_.conn_);
            }
        }
    };
    
    // Query awaiter for executing SQL queries
    class query_awaiter {
    private:
        connection& conn_;
        std::string query_;
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
            
            // Check if we can consume input without blocking
            if (PQconsumeInput(conn_.conn_) == 0) {
                throw query_error(conn_.conn_);
            }
            
            // Check if we can get a result without blocking
            if (!PQisBusy(conn_.conn_)) {
                PGresult* res = PQgetResult(conn_.conn_);
                result result_obj(res);
                
                // Flush all remaining results (normally there should be none for a single query)
                while ((res = PQgetResult(conn_.conn_)) != nullptr) {
                    PQclear(res);
                }
                
                return true; // Query completed synchronously
            }
            
            return false; // Need to await
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Create and start the query state object
            state_ = std::make_shared<pg_query_state>(conn_, query_, handle);
            state_->start();
        }
        
        result await_resume() {
            if (!state_ || !state_->result_) {
                throw query_error("Query failed");
            }
            
            return std::move(state_->result_);
        }
    };
    
    // Parameterized query awaiter
    class param_query_awaiter {
    private:
        connection& conn_;
        std::string query_;
        std::vector<std::string> param_values_;
        std::shared_ptr<pg_param_query_state> state_;
        
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
            
            // Check if we can consume input without blocking
            if (PQconsumeInput(conn_.conn_) == 0) {
                throw query_error(conn_.conn_);
            }
            
            // Check if we can get a result without blocking
            if (!PQisBusy(conn_.conn_)) {
                PGresult* res = PQgetResult(conn_.conn_);
                result result_obj(res);
                
                // Flush all remaining results (normally there should be none for a single query)
                while ((res = PQgetResult(conn_.conn_)) != nullptr) {
                    PQclear(res);
                }
                
                return true; // Query completed synchronously
            }
            
            return false; // Need to await
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Create and start the parameterized query state object
            state_ = std::make_shared<pg_param_query_state>(conn_, query_, param_values_, handle);
            state_->start();
        }
        
        result await_resume() {
            if (!state_ || !state_->result_) {
                throw query_error("Query failed");
            }
            
            return std::move(state_->result_);
        }
        
    private:
        // Convert vector of std::string to const char* array for libpq
        std::vector<const char*> get_param_values() const {
            std::vector<const char*> values;
            values.reserve(param_values_.size());
            for (const auto& param : param_values_) {
                values.push_back(param.c_str());
            }
            return values;
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
    
    // Static method that implements async_compose pattern
    template <typename CompletionToken>
    static auto async_connect(
        boost::asio::io_context& io,
        const std::string& conninfo,
        CompletionToken&& token) {
            
        return boost::asio::async_compose<CompletionToken, void(std::exception_ptr, std::shared_ptr<connection>)>(
            [io_ptr = &io, conninfo](auto& self) {
                boost::asio::co_spawn(*io_ptr, [io_ptr, conninfo, &self]() -> boost::asio::awaitable<void> {
                    try {
                        // Create a connection object
                        auto conn = std::make_shared<connection>(*io_ptr);
                        
                        // Connect to the database
                        co_await conn->connect(conninfo);
                        
                        // Call the completion handler with success
                        self.complete(std::exception_ptr(), conn);
                    } catch (...) {
                        // Call the completion handler with the exception
                        self.complete(std::current_exception(), nullptr);
                    }
                    
                    co_return;
                }, boost::asio::detached);
            },
            token,
            io
        );
    }
    
    // Static method that implements async_compose pattern for query execution
    template <typename CompletionToken>
    static auto async_query(
        std::shared_ptr<connection> conn,
        const std::string& query_text,
        CompletionToken&& token) {
            
        return boost::asio::async_compose<CompletionToken, void(std::exception_ptr, result)>(
            [conn, query_text](auto& self) {
                boost::asio::co_spawn(conn->io_, [conn, query_text, &self]() -> boost::asio::awaitable<void> {
                    try {
                        // Execute the query
                        auto res = co_await conn->query(query_text);
                        
                        // Call the completion handler with success
                        self.complete(std::exception_ptr(), std::move(res));
                    } catch (...) {
                        // Call the completion handler with the exception
                        self.complete(std::current_exception(), result());
                    }
                    
                    co_return;
                }, boost::asio::detached);
            },
            token,
            conn->io_
        );
    }
    
    // Static method that implements async_compose pattern for parameterized query execution
    template <typename CompletionToken>
    static auto async_query_params(
        std::shared_ptr<connection> conn,
        const std::string& query_text,
        const std::vector<std::string>& params,
        CompletionToken&& token) {
            
        return boost::asio::async_compose<CompletionToken, void(std::exception_ptr, result)>(
            [conn, query_text, params](auto& self) {
                boost::asio::co_spawn(conn->io_, [conn, query_text, params, &self]() -> boost::asio::awaitable<void> {
                    try {
                        // Execute the parameterized query
                        auto res = co_await conn->query_params(query_text, params);
                        
                        // Call the completion handler with success
                        self.complete(std::exception_ptr(), std::move(res));
                    } catch (...) {
                        // Call the completion handler with the exception
                        self.complete(std::current_exception(), result());
                    }
                    
                    co_return;
                }, boost::asio::detached);
            },
            token,
            conn->io_
        );
    }
};

// Implementation of state objects

// Connect state implementation
void pg_connect_state::start() {
    poll_connection();
}

void pg_connect_state::poll_connection() {
    PostgresPollingStatusType poll_status = PQconnectPoll(conn_.native_handle());
    
    if (poll_status == PGRES_POLLING_FAILED) {
        complete_ = false;
        handle_.resume();
        return;
    }
    
    if (poll_status == PGRES_POLLING_OK) {
        complete_ = true;
        handle_.resume();
        return;
    }
    
    // Need more polling
    poll_write_ = (poll_status == PGRES_POLLING_WRITING);
    poll_read_ = (poll_status == PGRES_POLLING_READING);
    
    if (poll_read_) {
        conn_.socket().async_wait(
            boost::asio::posix::descriptor_base::wait_read,
            [self = shared_from_this()](const boost::system::error_code& ec) {
                self->on_socket_ready(ec);
            }
        );
    } else if (poll_write_) {
        conn_.socket().async_wait(
            boost::asio::posix::descriptor_base::wait_write,
            [self = shared_from_this()](const boost::system::error_code& ec) {
                self->on_socket_ready(ec);
            }
        );
    }
}

void pg_connect_state::on_socket_ready(const boost::system::error_code& ec) {
    if (ec) {
        complete_ = false;
        handle_.resume();
        return;
    }
    
    poll_connection();
}

// Query state implementation
void pg_query_state::start() {
    wait_for_result();
}

void pg_query_state::wait_for_result() {
    conn_.socket().async_wait(
        boost::asio::posix::descriptor_base::wait_read,
        [self = shared_from_this()](const boost::system::error_code& ec) {
            self->on_socket_ready(ec);
        }
    );
}

void pg_query_state::on_socket_ready(const boost::system::error_code& ec) {
    if (ec) {
        handle_.resume();
        return;
    }
    
    // Process available input
    if (PQconsumeInput(conn_.native_handle()) == 0) {
        handle_.resume();
        return;
    }
    
    // Check if we can get a result
    if (!PQisBusy(conn_.native_handle())) {
        PGresult* res = PQgetResult(conn_.native_handle());
        result_ = result(res);
        
        // Flush all remaining results
        while ((res = PQgetResult(conn_.native_handle())) != nullptr) {
            PQclear(res);
        }
        
        handle_.resume();
    } else {
        // Still busy, continue waiting
        wait_for_result();
    }
}

// Parameterized query state implementation
void pg_param_query_state::start() {
    wait_for_result();
}

void pg_param_query_state::wait_for_result() {
    conn_.socket().async_wait(
        boost::asio::posix::descriptor_base::wait_read,
        [self = shared_from_this()](const boost::system::error_code& ec) {
            self->on_socket_ready(ec);
        }
    );
}

void pg_param_query_state::on_socket_ready(const boost::system::error_code& ec) {
    if (ec) {
        handle_.resume();
        return;
    }
    
    // Process available input
    if (PQconsumeInput(conn_.native_handle()) == 0) {
        handle_.resume();
        return;
    }
    
    // Check if we can get a result
    if (!PQisBusy(conn_.native_handle())) {
        PGresult* res = PQgetResult(conn_.native_handle());
        result_ = result(res);
        
        // Flush all remaining results
        while ((res = PQgetResult(conn_.native_handle())) != nullptr) {
            PQclear(res);
        }
        
        handle_.resume();
    } else {
        // Still busy, continue waiting
        wait_for_result();
    }
}

// Convenience functions that use the async_compose pattern

// Connect to a PostgreSQL database
template <typename CompletionToken>
auto async_connect(
    boost::asio::io_context& io,
    const std::string& conninfo,
    CompletionToken&& token) {
    
    return connection::async_connect(io, conninfo, std::forward<CompletionToken>(token));
}

// Execute a SQL query
template <typename CompletionToken>
auto async_query(
    std::shared_ptr<connection> conn,
    const std::string& query_text,
    CompletionToken&& token) {
    
    return connection::async_query(conn, query_text, std::forward<CompletionToken>(token));
}

// Execute a parameterized SQL query
template <typename CompletionToken>
auto async_query_params(
    std::shared_ptr<connection> conn,
    const std::string& query_text,
    const std::vector<std::string>& params,
    CompletionToken&& token) {
    
    return connection::async_query_params(conn, query_text, params, std::forward<CompletionToken>(token));
}

} // namespace pg_async