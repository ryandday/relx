#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <expected>
#include <vector>
#include <coroutine>
#include <tuple>
#include <type_traits>

#include <boost/pfr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "connection.hpp"
#include "../results/result.hpp"
#include "../query/core.hpp"

// Forward declarations to avoid including libpq headers in our public API
struct pg_conn;
typedef struct pg_conn PGconn;
struct pg_result;
typedef struct pg_result PGresult;

namespace relx {
namespace connection {

/// @brief Async PostgreSQL implementation of the Connection interface using Boost.Asio
class PostgreSQLAsyncConnection {
public:
    /// @brief Constructor with connection parameters and io_context
    /// @param io_context Boost.Asio io_context for async operations
    /// @param connection_string PostgreSQL connection string
    explicit PostgreSQLAsyncConnection(
        boost::asio::io_context& io_context,
        std::string_view connection_string);
    
    /// @brief Destructor that ensures proper cleanup
    ~PostgreSQLAsyncConnection();
    
    // Delete copy constructor and assignment operator
    PostgreSQLAsyncConnection(const PostgreSQLAsyncConnection&) = delete;
    PostgreSQLAsyncConnection& operator=(const PostgreSQLAsyncConnection&) = delete;

    /// @brief Move constructor
    PostgreSQLAsyncConnection(PostgreSQLAsyncConnection&& other) = delete;;

    /// @brief Move assignment operator
    PostgreSQLAsyncConnection& operator=(PostgreSQLAsyncConnection&& other) = delete;

    /// @brief Connect to the PostgreSQL database asynchronously
    /// @return Awaitable for the async operation
    boost::asio::awaitable<ConnectionResult<void>> connect();

    /// @brief Disconnect from the PostgreSQL database asynchronously
    /// @return Awaitable for the async operation
    boost::asio::awaitable<ConnectionResult<void>> disconnect();

    /// @brief Execute a raw SQL query with parameters asynchronously
    /// @param sql The SQL query string
    /// @param params Vector of parameter values
    /// @return Awaitable containing the query results or an error
    boost::asio::awaitable<ConnectionResult<result::ResultSet>> execute_raw(
        const std::string& sql, 
        const std::vector<std::string>& params = {});

    /// @brief Execute a query expression asynchronously
    /// @param query The query expression to execute
    /// @return Awaitable containing the query results or an error
    template <query::SqlExpr Query>
    boost::asio::awaitable<ConnectionResult<result::ResultSet>> execute(const Query& query) {
        std::string sql = query.to_sql();
        std::vector<std::string> params = query.bind_params();
        return execute_raw(sql, params);
    }

    /// @brief Execute a query and map results to a user-defined type asynchronously
    /// @tparam T The user-defined type to map results to
    /// @tparam Query The query expression type
    /// @param query The query expression to execute
    /// @return Awaitable containing the mapped user-defined type or an error
    template <typename T, query::SqlExpr Query>
    boost::asio::awaitable<ConnectionResult<T>> execute(const Query& query) {
        auto result = co_await execute(query);
        if (!result) {
            co_return std::unexpected(result.error());
        }

        const auto& resultSet = *result;
        if (resultSet.empty()) {
            co_return std::unexpected(ConnectionError{.message = "No results found"});
        }

        // Create an instance of T
        T obj{};
        
        // Get the first row of results
        const auto& row = resultSet.at(0);
        
        // Use Boost.PFR to get the tuple type that matches our struct
        auto structure_tie = boost::pfr::structure_tie(obj);
        
        // Make sure the number of columns matches the number of fields in the struct
        if (resultSet.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
            co_return std::unexpected(ConnectionError{
                .message = "Column count does not match struct field count",
                .error_code = -1
            });
        }
        
        // Convert each value in the result row to the appropriate type in the struct
        try {
            // Create a vector of values from the row
            std::vector<std::string> values;
            for (size_t i = 0; i < resultSet.column_count(); ++i) {
                auto cell_result = row.get_cell(i);
                if (!cell_result) {
                    co_return std::unexpected(ConnectionError{
                        .message = "Failed to get cell value: " + cell_result.error().message,
                        .error_code = -1
                    });
                }
                values.push_back((*cell_result)->raw_value());
            }
            
            // Apply tuple assignment from the row values to the struct fields
            map_row_to_tuple(structure_tie, values);
        } catch (const std::exception& e) {
            co_return std::unexpected(ConnectionError{
                .message = std::string("Failed to convert result to struct: ") + e.what(),
                .error_code = -1
            });
        }
        
        co_return obj;
    }

    /// @brief Execute a query and map results to a vector of user-defined types asynchronously
    /// @tparam T The user-defined type to map results to
    /// @tparam Query The query expression type
    /// @param query The query expression to execute
    /// @return Awaitable containing a vector of mapped user-defined types or an error
    template <typename T, query::SqlExpr Query>
    boost::asio::awaitable<ConnectionResult<std::vector<T>>> execute_many(const Query& query) {
        auto result = co_await execute(query);
        if (!result) {
            co_return std::unexpected(result.error());
        }

        const auto& resultSet = *result;
        std::vector<T> objects;
        objects.reserve(resultSet.size());
        
        // Check if we have at least one row to determine column count
        if (resultSet.empty()) {
            co_return objects;  // Return empty vector
        }
        
        // Make sure the number of columns matches the number of fields in the struct
        if (resultSet.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
            co_return std::unexpected(ConnectionError{
                .message = "Column count does not match struct field count",
                .error_code = -1
            });
        }

        // Process each row
        for (size_t row_idx = 0; row_idx < resultSet.size(); ++row_idx) {
            const auto& row = resultSet.at(row_idx);
            T obj{};
            auto structure_tie = boost::pfr::structure_tie(obj);
            
            try {
                // Create a vector of values from the row
                std::vector<std::string> values;
                for (size_t i = 0; i < resultSet.column_count(); ++i) {
                    auto cell_result = row.get_cell(i);
                    if (!cell_result) {
                        co_return std::unexpected(ConnectionError{
                            .message = "Failed to get cell value: " + cell_result.error().message,
                            .error_code = -1
                        });
                    }
                    values.push_back((*cell_result)->raw_value());
                }
                
                map_row_to_tuple(structure_tie, values);
                objects.push_back(std::move(obj));
            } catch (const std::exception& e) {
                co_return std::unexpected(ConnectionError{
                    .message = std::string("Failed to convert result to struct: ") + e.what(),
                    .error_code = -1
                });
            }
        }
        
        co_return objects;
    }

    /// @brief Check if the connection is open
    /// @return True if connected, false otherwise
    bool is_connected() const;
    
    /// @brief Begin a new transaction asynchronously
    /// @param isolation_level The isolation level for the transaction
    /// @return Awaitable indicating success or failure
    boost::asio::awaitable<ConnectionResult<void>> begin_transaction(
        IsolationLevel isolation_level = IsolationLevel::ReadCommitted);
    
    /// @brief Commit the current transaction asynchronously
    /// @return Awaitable indicating success or failure
    boost::asio::awaitable<ConnectionResult<void>> commit_transaction();
    
    /// @brief Rollback the current transaction asynchronously
    /// @return Awaitable indicating success or failure
    boost::asio::awaitable<ConnectionResult<void>> rollback_transaction();
    
    /// @brief Check if a transaction is currently active
    /// @return True if a transaction is active, false otherwise
    bool in_transaction() const;

    /// @brief Get direct access to the PostgreSQL connection
    /// @return The PGconn pointer
    PGconn* get_pg_conn() const { return pg_conn_; }

    /// @brief Get a reference to the socket
    /// @return Reference to the socket
    boost::asio::ip::tcp::socket& get_socket() { return socket_; }

private:
    std::string connection_string_;
    PGconn* pg_conn_ = nullptr;
    bool is_connected_ = false;
    bool in_transaction_ = false;
    
    // Socket for async operations
    boost::asio::ip::tcp::socket socket_;

    /// @brief Helper method to handle PGresult and convert to ConnectionResult
    /// @param result PGresult pointer to process
    /// @param expected_status Expected status code (or -1 to ignore)
    /// @return ConnectionResult with error or success
    ConnectionResult<PGresult*> handle_pg_result(PGresult* result, int expected_status = -1);
    
    /// @brief Convert SQL with ? placeholders to PostgreSQL's $n format
    /// @param sql SQL query with ? placeholders
    /// @return Converted SQL with $1, $2, etc. placeholders
    std::string convert_placeholders(const std::string& sql);

    /// @brief Helper to wait for a PostgreSQL socket to be readable
    /// @return Awaitable for the async operation
    boost::asio::awaitable<ConnectionResult<void>> wait_for_readable();

    /// @brief Helper to wait for a PostgreSQL socket to be writable
    /// @return Awaitable for the async operation
    boost::asio::awaitable<ConnectionResult<void>> wait_for_writable();
    
    /// @brief Helper to poll a PostgreSQL connection during connect
    /// @return Awaitable for the connect operation
    boost::asio::awaitable<ConnectionResult<void>> poll_connection();
    
    /// @brief Helper to process PostgreSQL query results asynchronously
    /// @return Awaitable containing the query results
    boost::asio::awaitable<ConnectionResult<result::ResultSet>> process_query_result();

    /// @brief Helper function to map a result row to a tuple (and thus to a struct)
    /// @tparam Tuple The tuple type that corresponds to the struct fields
    /// @param tuple The tuple to fill with values
    /// @param row The database result row containing values
    template <typename Tuple>
    void map_row_to_tuple(Tuple& tuple, const std::vector<std::string>& row) {
        // Apply tuple assignment using fold expressions and parameter pack expansion
        apply_tuple_assignment(tuple, row, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
    }
    
    /// @brief Helper function to perform the tuple assignment with index sequence
    /// @tparam Tuple The tuple type
    /// @tparam Indices Parameter pack for indices
    /// @param tuple The tuple to assign to
    /// @param row The data row
    /// @param indices Index sequence for tuple elements
    template <typename Tuple, size_t... Indices>
    void apply_tuple_assignment(Tuple& tuple, const std::vector<std::string>& row, std::index_sequence<Indices...>) {
        // For each index in the tuple, convert the string value to the appropriate type
        (convert_and_assign(std::get<Indices>(tuple), row[Indices]), ...);
    }

    /// @brief Convert a string value to the target type and assign it
    /// @tparam T The target type
    /// @param target The target variable
    /// @param value The string value to convert
    template <typename T>
    void convert_and_assign(T& target, const std::string& value) {
        // This is a simplified version - real implementation would handle more types
        if constexpr (std::is_same_v<T, std::string>) {
            target = value;
        } else if constexpr (std::is_same_v<T, bool>) {
            target = (value == "t" || value == "true" || value == "1");
        } else if constexpr (std::is_integral_v<T>) {
            target = static_cast<T>(std::stoll(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            target = static_cast<T>(std::stod(value));
        }
        // Add more type conversions as needed
    }
};

} // namespace connection
} // namespace relx 