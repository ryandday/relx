#pragma once

#include "core.hpp"
#include "column_expression.hpp"
#include "../schema/table.hpp"
#include "../schema/column.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>

namespace sqllib {
namespace query {

/// @brief Adapter to convert schema::column to a ColumnRef
/// This allows direct use of schema columns in query expressions
template <ColumnType C>
class SchemaColumnAdapter : public ColumnExpression {
public:
    using value_type = typename C::value_type;
    
    explicit SchemaColumnAdapter(const C& col, std::string_view table_name = "") 
        : col_(col), table_name_(table_name) {}
    
    std::string to_sql() const override {
        return qualified_name();
    }
    
    std::vector<std::string> bind_params() const override {
        return {};
    }
    
    std::string column_name() const override {
        return std::string(C::name);
    }
    
    std::string table_name() const override {
        return std::string(table_name_);
    }
    
    const C& column() const {
        return col_;
    }
    
private:
    const C& col_;
    std::string_view table_name_;
};

/// @brief Adapter to convert schema::table to work with query builder
/// This maintains table name and enables column access with SQL expression support
template <TableType T>
class SchemaTableAdapter {
public:
    static constexpr auto table_name = T::table_name;
    
    explicit SchemaTableAdapter(const T& table) : table_(table) {}
    
    /// Get a column from this table as a SQL expression
    template <ColumnType C>
    auto get_column(const C& col) const {
        return SchemaColumnAdapter<C>(col, table_name);
    }
    
    const T& schema_table() const {
        return table_;
    }
    
private:
    const T& table_;
};

/// @brief Helper to wrap a schema column in a SQL expression
/// @tparam C The column type
/// @param col The column to wrap
/// @return A SchemaColumnAdapter that implements the SqlExpr concept
template <ColumnType C>
auto to_expr(const C& col, std::string_view table_name = "") {
    return SchemaColumnAdapter<C>(col, table_name);
}

/// @brief Helper to wrap a schema table in a table adapter
/// @tparam T The table type
/// @param table The table to wrap
/// @return A SchemaTableAdapter that implements table operations
template <TableType T>
auto to_table(const T& table) {
    return SchemaTableAdapter<T>(table);
}

} // namespace query
} // namespace sqllib 