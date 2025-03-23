#pragma once

#include "core.hpp"
#include "column_expression.hpp"
#include "condition.hpp"
#include "value.hpp"
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <tuple>
#include <utility>
#include <optional>
#include <iostream>

namespace sqllib {
namespace query {

/// @brief Represents a SET clause assignment in an UPDATE statement
template <ColumnType Column, SqlExpr Value>
struct SetItem {
    ColumnRef<Column> column;
    Value value;

    // Constructor to ensure the SetItem can be properly initialized
    SetItem(ColumnRef<Column> col, Value val) : column(std::move(col)), value(std::move(val)) {}

    std::string to_sql() const {
        return column.column_name() + " = " + value.to_sql();
    }

    std::vector<std::string> bind_params() const {
        return value.bind_params();
    }
};

/// @brief Base UPDATE query builder
/// @tparam Table Table to update
/// @tparam Sets Tuple of SET clause items
/// @tparam Where Optional where condition
template <
    TableType Table,
    typename Sets = std::tuple<>,
    typename Where = std::nullopt_t
>
class UpdateQuery {
private:
    // Helper to check if a tuple is empty
    template <typename Tuple>
    static constexpr bool is_empty_tuple() {
        return std::tuple_size_v<Tuple> == 0;
    }
    
    // Helper to convert a tuple of SET items to SQL
    template <typename Tuple>
    std::string tuple_to_sql(const Tuple& tuple, const char* separator) const {
        std::stringstream ss;
        int i = 0;
        std::apply([&](const auto&... items) {
            ((ss << (i++ > 0 ? separator : "") << items.to_sql()), ...);
        }, tuple);
        return ss.str();
    }
    
    // Helper to collect bind parameters from a tuple of expressions
    template <typename Tuple>
    std::vector<std::string> tuple_bind_params(const Tuple& tuple) const {
        std::vector<std::string> params;
        
        std::apply([&](const auto&... items) {
            auto process_item = [&params](const auto& item) {
                auto item_params = item.bind_params();
                params.insert(params.end(), item_params.begin(), item_params.end());
            };
            
            (process_item(items), ...);
        }, tuple);
        
        return params;
    }

public:
    using table_type = Table;
    using sets_type = Sets;
    using where_type = Where;

    /// @brief Constructor for the UPDATE query builder
    /// @param table The table to update
    /// @param sets The SET clause items
    /// @param where The WHERE condition
    explicit UpdateQuery(
        Table table,
        Sets sets = {},
        Where where = std::nullopt
    ) : table_(std::move(table)),
        sets_(std::move(sets)),
        where_(std::move(where)) {}

    /// @brief Generate the SQL for this UPDATE query
    /// @return The SQL string
    std::string to_sql() const {
        std::stringstream ss;
        ss << "UPDATE " << table_.table_name;
        
        // Add SET clause
        if constexpr (!is_empty_tuple<Sets>()) {
            ss << " SET ";
            ss << tuple_to_sql(sets_, ", ");
        }
        
        // Add WHERE clause
        if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
            if (where_.has_value()) {
                ss << " WHERE " << where_.value().to_sql();
            }
        }
        
        return ss.str();
    }

    /// @brief Get the bind parameters for this UPDATE query
    /// @return Vector of bind parameters
    std::vector<std::string> bind_params() const {
        std::vector<std::string> params;
        
        // Collect parameters from SET items
        if constexpr (!is_empty_tuple<Sets>()) {
            auto set_params = tuple_bind_params(sets_);
            params.insert(params.end(), set_params.begin(), set_params.end());
        }
        
        // Collect where parameter
        if constexpr (!std::is_same_v<Where, std::nullopt_t>) {
            if (where_.has_value()) {
                auto where_params = where_.value().bind_params();
                params.insert(params.end(), where_params.begin(), where_params.end());
            }
        }
        
        return params;
    }

    /// @brief Add or replace a SET clause assignment
    /// @tparam Col The column type
    /// @tparam Val The value expression type
    /// @param column The column to set
    /// @param val The value to set
    /// @return New UpdateQuery with the SET clause added
    template <ColumnType Col, SqlExpr Val>
    auto set(const Col& column, const Val& val) const {
        using SetItemType = SetItem<Col, Val>;
        
        // Create a new SetItem
        auto column_ref = ColumnRef<Col>(column);
        auto set_item = SetItemType(column_ref, val);
        
        // Create a tuple with the new SetItem
        auto new_item_tuple = std::make_tuple(set_item);
        
        // Concatenate with existing sets
        auto new_sets = std::tuple_cat(sets_, new_item_tuple);
        
        // Return a new query with the updated sets
        return UpdateQuery<Table, decltype(new_sets), Where>(
            table_,
            std::move(new_sets),
            where_
        );
    }

    /// @brief Add a WHERE clause to the query
    /// @tparam Condition The condition type
    /// @param cond The WHERE condition
    /// @return New UpdateQuery with the WHERE clause added
    template <ConditionExpr Condition>
    auto where(const Condition& cond) const {
        return UpdateQuery<
            Table, Sets, std::optional<Condition>
        >(
            table_,
            sets_,
            std::optional<Condition>(cond)
        );
    }

private:
    Table table_;
    Sets sets_;
    Where where_;
};

/// @brief Create an UPDATE query for the specified table
/// @tparam Table The table type
/// @param table The table to update
/// @return An UpdateQuery object
template <TableType Table>
auto update(const Table& table) {
    return UpdateQuery<Table>(table);
}

} // namespace query
} // namespace sqllib 