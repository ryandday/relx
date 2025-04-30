#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <expected>
#include <vector>
#include <tuple>
#include <type_traits>

#include <boost/pfr.hpp>

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

    /// @brief Execute a query and map results to a user-defined type using Boost.PFR
    /// @note The struct must be an aggregate type (has no virtual functions or private members)
    /// @note The struct should have public members in the same order as the columns in the result set
    /// @tparam T The user-defined type to map results to
    /// @tparam Query The query expression type
    /// @param query The query expression to execute
    /// @return Result containing the mapped user-defined type or an error
    template <typename T, query::SqlExpr Query>
    ConnectionResult<T> execute(const Query& query) {
        auto result = execute(query);
        if (!result) {
            return std::unexpected(result.error());
        }

        const auto& resultSet = *result;
        if (resultSet.empty()) {
            return std::unexpected(ConnectionError{.message = "No results found"});
        }

        // Create an instance of T
        T obj{};
        
        // Get the first row of results
        const auto& row = resultSet.at(0);
        
        // Use Boost.PFR to get the tuple type that matches our struct
        auto structure_tie = boost::pfr::structure_tie(obj);
        
        // Make sure the number of columns matches the number of fields in the struct
        // TODO What if struct has less fields than columns? Why is it working?
        if (resultSet.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
            return std::unexpected(ConnectionError{
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
                    return std::unexpected(ConnectionError{
                        .message = "Failed to get cell value: " + cell_result.error().message,
                        .error_code = -1
                    });
                }
                values.push_back((*cell_result)->raw_value());
            }
            
            // Apply tuple assignment from the row values to the struct fields
            map_row_to_tuple(structure_tie, values);
        } catch (const std::exception& e) {
            return std::unexpected(ConnectionError{
                .message = std::string("Failed to convert result to struct: ") + e.what(),
                .error_code = -1
            });
        }
        
        return obj;
    }

    /// @brief Execute a query and map results to a vector of user-defined types
    /// @tparam T The user-defined type to map results to
    /// @tparam Query The query expression type
    /// @param query The query expression to execute
    /// @return Result containing a vector of mapped user-defined types or an error
    template <typename T, query::SqlExpr Query>
    ConnectionResult<std::vector<T>> execute_many(const Query& query) {
        auto result = execute(query);
        if (!result) {
            return std::unexpected(result.error());
        }

        const auto& resultSet = *result;
        std::vector<T> objects;
        objects.reserve(resultSet.size());
        
        // Check if we have at least one row to determine column count
        if (resultSet.empty()) {
            return objects;  // Return empty vector
        }
        
        // Make sure the number of columns matches the number of fields in the struct
        // TODO
        if (resultSet.column_count() != boost::pfr::tuple_size_v<std::remove_cvref_t<T>>) {
            return std::unexpected(ConnectionError{
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
                        return std::unexpected(ConnectionError{
                            .message = "Failed to get cell value: " + cell_result.error().message,
                            .error_code = -1
                        });
                    }
                    values.push_back((*cell_result)->raw_value());
                }
                
                map_row_to_tuple(structure_tie, values);
                objects.push_back(std::move(obj));
            } catch (const std::exception& e) {
                return std::unexpected(ConnectionError{
                    .message = std::string("Failed to convert result to struct: ") + e.what(),
                    .error_code = -1
                });
            }
        }
        
        return objects;
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

private:
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
    void convert_and_assign(T &target, const std::string &value) {
      if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> || std::is_same_v<T, char> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>) {
        target = value;
      } else if constexpr (std::is_same_v<T, bool>) {
        // Handle PostgreSQL-style boolean values ('t', 'f', etc.)
        target = (value == "1" || value == "true" || value == "TRUE" ||
                  value == "True" || value == "t" || value == "T" ||
                  value == "yes" || value == "YES" || value == "Y");
      } else if constexpr (std::is_integral_v<T>) {
        // // Handle empty strings
        // if (value.empty()) {
        //   target = 0;
        // } else {
        target = static_cast<T>(std::stoll(value));
    //   }
    }
    else if constexpr (std::is_floating_point_v<T>) {
    //   if (value.empty()) {
    //     target = 0.0;
    //   } else {
        target = std::stod(value);
    //   }
    }
    else {
      static_assert(std::is_same_v<T, bool>, "Unsupported type conversion");
      // needed
    }
    }
};

} // namespace connection

// Convenient imports from the connection namespace
using connection::Connection;
using connection::ConnectionError;
using connection::ConnectionResult;
using connection::IsolationLevel;

} // namespace relx