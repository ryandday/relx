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
#include <unordered_map>

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

class transaction_error : public pg_error {
public:
    explicit transaction_error(const std::string& msg) : pg_error(msg) {}
    explicit transaction_error(const char* msg) : pg_error(msg) {}
    explicit transaction_error(PGconn* conn) 
        : pg_error(PQerrorMessage(conn) ? PQerrorMessage(conn) : "Unknown PostgreSQL transaction error") {}
};

class statement_error : public pg_error {
public:
    explicit statement_error(const std::string& msg) : pg_error(msg) {}
    explicit statement_error(const char* msg) : pg_error(msg) {}
    explicit statement_error(PGconn* conn) 
        : pg_error(PQerrorMessage(conn) ? PQerrorMessage(conn) : "Unknown PostgreSQL statement error") {}
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

// Transaction isolation level
enum class IsolationLevel {
    ReadUncommitted,
    ReadCommitted,
    RepeatableRead,
    Serializable
};

// Forward declaration
class connection;

// Prepared statement class
class prepared_statement {
private:
    connection& conn_;
    std::string name_;
    std::string query_;
    bool prepared_ = false;

public:
    prepared_statement(connection& conn, std::string name, std::string query)
        : conn_(conn), name_(std::move(name)), query_(std::move(query)) {}

    ~prepared_statement() = default;

    // Non-copyable
    prepared_statement(const prepared_statement&) = delete;
    prepared_statement& operator=(const prepared_statement&) = delete;

    // Move constructible/assignable
    prepared_statement(prepared_statement&& other) noexcept
        : conn_(other.conn_), name_(std::move(other.name_)), 
          query_(std::move(other.query_)), prepared_(other.prepared_) {
        other.prepared_ = false;
    }

    prepared_statement& operator=(prepared_statement&& other) noexcept {
        if (this != &other) {
            name_ = std::move(other.name_);
            query_ = std::move(other.query_);
            prepared_ = other.prepared_;
            other.prepared_ = false;
        }
        return *this;
    }

    const std::string& name() const { return name_; }
    const std::string& query() const { return query_; }
    bool is_prepared() const { return prepared_; }

    // The implementation of prepare and execute will be defined in separate cpp file
    boost::asio::awaitable<void> prepare();
    boost::asio::awaitable<result> execute(const std::vector<std::string>& params);
    boost::asio::awaitable<void> deallocate();

    friend class connection;
};

// ----------------------------------------------------------------------
// The connection class - main interface for PostgreSQL operations
// ----------------------------------------------------------------------
class connection {
private:
    boost::asio::io_context& io_;
    PGconn* conn_ = nullptr;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unordered_map<std::string, std::shared_ptr<prepared_statement>> statements_;
    bool in_transaction_ = false;
    
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
    
    // Asynchronous flush of outgoing data to the PostgreSQL server
    boost::asio::awaitable<void> flush_outgoing_data() {
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
    }
    
    // Helper method to wait for and get a query result
    boost::asio::awaitable<result> get_query_result() {
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
        : io_(other.io_), conn_(other.conn_), socket_(std::move(other.socket_)),
          statements_(std::move(other.statements_)), in_transaction_(other.in_transaction_) {
        other.conn_ = nullptr;
        other.in_transaction_ = false;
    }
    
    connection& operator=(connection&& other) noexcept {
        if (this != &other) {
            close();
            conn_ = other.conn_;
            socket_ = std::move(other.socket_);
            statements_ = std::move(other.statements_);
            in_transaction_ = other.in_transaction_;
            other.conn_ = nullptr;
            other.in_transaction_ = false;
        }
        return *this;
    }
    
    void close() {
        statements_.clear();
        
        if (socket_) {
            socket_->close();
            socket_.reset();
        }
        
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
        
        in_transaction_ = false;
    }
    
    bool is_open() const {
        return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
    }
    
    bool in_transaction() const {
        return in_transaction_;
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
    
    // Asynchronous parameterized query execution using boost::asio::awaitable
    boost::asio::awaitable<result> query(const std::string& query_text, const std::vector<std::string>& params = {}) {
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
                // TODO allow user to customize fields with nullptr values
                nullptr, // param types - inferred
                values.data(),
                nullptr, // param lengths - null-terminated strings
                nullptr, // param formats - text format
                0 // result format - text format
            )) {
            throw query_error(conn_);
        }
        
        // Flush the outgoing data
        co_await flush_outgoing_data();
        
        // Get and return the result
        co_return co_await get_query_result();
    }
    
    // Transaction support
    // ------------------
    
    // Begin transaction with specified isolation level
    boost::asio::awaitable<void> begin_transaction(IsolationLevel isolation = IsolationLevel::ReadCommitted) {
        if (in_transaction_) {
            throw transaction_error("Already in a transaction");
        }
        
        std::string isolation_str;
        switch (isolation) {
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
        
        std::string begin_cmd = "BEGIN ISOLATION LEVEL " + isolation_str;
        result res = co_await query(begin_cmd);
        
        if (!res) {
            throw transaction_error(res.error_message());
        }
        
        in_transaction_ = true;
        co_return;
    }
    
    // Commit the current transaction
    boost::asio::awaitable<void> commit() {
        if (!in_transaction_) {
            throw transaction_error("Not in a transaction");
        }
        
        result res = co_await query("COMMIT");
        
        if (!res) {
            throw transaction_error(res.error_message());
        }
        
        in_transaction_ = false;
        co_return;
    }
    
    // Rollback the current transaction
    boost::asio::awaitable<void> rollback() {
        if (!in_transaction_) {
            throw transaction_error("Not in a transaction");
        }
        
        result res = co_await query("ROLLBACK");
        
        if (!res) {
            throw transaction_error(res.error_message());
        }
        
        in_transaction_ = false;
        co_return;
    }
    
    // Prepared statement support
    // -------------------------
    
    // Create a prepared statement
    boost::asio::awaitable<std::shared_ptr<prepared_statement>> prepare_statement(const std::string& name, const std::string& query_text) {
        if (!is_open()) {
            throw connection_error("Connection is not open");
        }
        
        auto it = statements_.find(name);
        if (it != statements_.end()) {
            // Statement with this name already exists
            if (it->second->query() != query_text) {
                // Deallocate the old statement since the query is different
                co_await it->second->deallocate();
                statements_.erase(it);
            } else {
                // Return the existing statement if it has the same query
                co_return it->second;
            }
        }
        
        // Create a new prepared statement
        auto stmt = std::make_shared<prepared_statement>(*this, name, query_text);
        statements_[name] = stmt;
        
        // Prepare it
        co_await stmt->prepare();
        
        co_return stmt;
    }
    
    // Get a prepared statement by name
    std::shared_ptr<prepared_statement> get_prepared_statement(const std::string& name) {
        auto it = statements_.find(name);
        if (it != statements_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // Execute a prepared statement by name
    boost::asio::awaitable<result> execute_prepared(const std::string& name, const std::vector<std::string>& params = {}) {
        auto stmt = get_prepared_statement(name);
        if (!stmt) {
            throw statement_error("Prepared statement not found: " + name);
        }
        
        co_return co_await stmt->execute(params);
    }
    
    // Deallocate a prepared statement by name
    boost::asio::awaitable<void> deallocate_prepared(const std::string& name) {
        auto stmt = get_prepared_statement(name);
        if (!stmt) {
            throw statement_error("Prepared statement not found: " + name);
        }
        
        co_await stmt->deallocate();
        statements_.erase(name);
        co_return;
    }
    
    // Deallocate all prepared statements
    boost::asio::awaitable<void> deallocate_all_prepared() {
        std::vector<std::string> names;
        names.reserve(statements_.size());
        
        for (auto& [name, _] : statements_) {
            names.push_back(name);
        }
        
        for (const auto& name : names) {
            co_await deallocate_prepared(name);
        }
        
        co_return;
    }
    
    friend class prepared_statement;
};

} // namespace relx::pgsql_async_wrapper