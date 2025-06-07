#pragma once

#include "../schema/core.hpp"
#include "../schema/table.hpp"
#include "../schema/column.hpp"
#include "../schema/fixed_string.hpp"

#include <string>
#include <vector>
#include <memory>
#include <type_traits>
#include <boost/pfr.hpp>

namespace relx::migrations {

/// @brief Enum for migration operation types
enum class OperationType {
    CREATE_TABLE,
    DROP_TABLE,
    ADD_COLUMN,
    DROP_COLUMN,
    RENAME_COLUMN,
    MODIFY_COLUMN,
    UPDATE_DATA,
    ADD_CONSTRAINT,
    DROP_CONSTRAINT,
    RENAME_CONSTRAINT,
    ADD_INDEX,
    DROP_INDEX
};

/// @brief Base class for migration operations
class MigrationOperation {
public:
    virtual ~MigrationOperation() = default;
    virtual std::string to_sql() const = 0;
    virtual std::string rollback_sql() const = 0;
    virtual OperationType type() const = 0;
    virtual std::vector<std::string> bind_params() const { return {}; }
    virtual std::vector<std::string> rollback_bind_params() const { return {}; }
};

/// @brief CREATE TABLE migration operation
template <schema::TableConcept Table>
class CreateTableOperation : public MigrationOperation {
private:
    const Table& table_;

public:
    explicit CreateTableOperation(const Table& table) : table_(table) {}

    std::string to_sql() const override {
        return schema::create_table(table_).to_sql();
    }

    std::string rollback_sql() const override {
        return schema::drop_table(table_).if_exists().to_sql();
    }

    OperationType type() const override {
        return OperationType::CREATE_TABLE;
    }
};

/// @brief DROP TABLE migration operation
template <schema::TableConcept Table>
class DropTableOperation : public MigrationOperation {
private:
    const Table& table_;

public:
    explicit DropTableOperation(const Table& table) : table_(table) {}

    std::string to_sql() const override {
        return schema::drop_table(table_).if_exists().to_sql();
    }

    std::string rollback_sql() const override {
        return schema::create_table(table_).to_sql();
    }

    OperationType type() const override {
        return OperationType::DROP_TABLE;
    }
};

/// @brief ADD COLUMN migration operation
template <typename Column>
class AddColumnOperation : public MigrationOperation {
private:
    std::string table_name_;
    Column column_;

public:
    AddColumnOperation(std::string table_name, const Column& column) 
        : table_name_(std::move(table_name)), column_(column) {}

    std::string to_sql() const override {
        return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition() + ";";
    }

    std::string rollback_sql() const override {
        return "ALTER TABLE " + table_name_ + " DROP COLUMN " + std::string(Column::name) + ";";
    }

    OperationType type() const override {
        return OperationType::ADD_COLUMN;
    }
};

/// @brief DROP COLUMN migration operation
template <typename Column>
class DropColumnOperation : public MigrationOperation {
private:
    std::string table_name_;
    Column column_;

public:
    DropColumnOperation(std::string table_name, const Column& column) 
        : table_name_(std::move(table_name)), column_(column) {}

    std::string to_sql() const override {
        return "ALTER TABLE " + table_name_ + " DROP COLUMN " + std::string(Column::name) + ";";
    }

    std::string rollback_sql() const override {
        return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition() + ";";
    }

    OperationType type() const override {
        return OperationType::DROP_COLUMN;
    }
};

/// @brief RENAME COLUMN migration operation
class RenameColumnOperation : public MigrationOperation {
private:
    std::string table_name_;
    std::string old_name_;
    std::string new_name_;

public:
    RenameColumnOperation(std::string table_name, std::string old_name, std::string new_name) 
        : table_name_(std::move(table_name)), old_name_(std::move(old_name)), new_name_(std::move(new_name)) {}

    std::string to_sql() const override {
        return "ALTER TABLE " + table_name_ + " RENAME COLUMN " + old_name_ + " TO " + new_name_ + ";";
    }

    std::string rollback_sql() const override {
        return "ALTER TABLE " + table_name_ + " RENAME COLUMN " + new_name_ + " TO " + old_name_ + ";";
    }

    OperationType type() const override {
        return OperationType::RENAME_COLUMN;
    }
};

/// @brief RENAME CONSTRAINT migration operation  
class RenameConstraintOperation : public MigrationOperation {
private:
    std::string table_name_;
    std::string old_name_;
    std::string new_name_;

public:
    RenameConstraintOperation(std::string table_name, std::string old_name, std::string new_name) 
        : table_name_(std::move(table_name)), old_name_(std::move(old_name)), new_name_(std::move(new_name)) {}

    std::string to_sql() const override {
        return "ALTER TABLE " + table_name_ + " RENAME CONSTRAINT " + old_name_ + " TO " + new_name_ + ";";
    }

    std::string rollback_sql() const override {
        return "ALTER TABLE " + table_name_ + " RENAME CONSTRAINT " + new_name_ + " TO " + old_name_ + ";";
    }

    OperationType type() const override {
        return OperationType::RENAME_CONSTRAINT;
    }
};

/// @brief UPDATE DATA migration operation for column transformations
class UpdateDataOperation : public MigrationOperation {
private:
    std::string table_name_;
    std::string target_column_;
    std::string source_column_;
    std::string forward_transform_;
    std::string backward_transform_;

public:
    UpdateDataOperation(std::string table_name, std::string target_column, std::string source_column,
                       std::string forward_transform, std::string backward_transform) 
        : table_name_(std::move(table_name)), target_column_(std::move(target_column)), 
          source_column_(std::move(source_column)), forward_transform_(std::move(forward_transform)),
          backward_transform_(std::move(backward_transform)) {}

    std::string to_sql() const override {
        return "UPDATE " + table_name_ + " SET " + target_column_ + " = " + forward_transform_ + ";";
    }

    std::string rollback_sql() const override {
        return "UPDATE " + table_name_ + " SET " + source_column_ + " = " + backward_transform_ + ";";
    }

    OperationType type() const override {
        return OperationType::UPDATE_DATA;
    }
};

/// @brief Container for migration operations
class Migration {
private:
    std::vector<std::unique_ptr<MigrationOperation>> operations_;
    std::string name_;

public:
    explicit Migration(std::string name) : name_(std::move(name)) {}

    /// @brief Add an operation to the migration
    template <typename Op, typename... Args>
    void add_operation(Args&&... args) {
        operations_.push_back(std::make_unique<Op>(std::forward<Args>(args)...));
    }

    /// @brief Generate forward migration SQL
    std::vector<std::string> forward_sql() const {
        std::vector<std::string> sqls;
        sqls.reserve(operations_.size());
        for (const auto& op : operations_) {
            sqls.push_back(op->to_sql());
        }
        return sqls;
    }

    /// @brief Generate rollback migration SQL
    std::vector<std::string> rollback_sql() const {
        std::vector<std::string> sqls;
        sqls.reserve(operations_.size());
        // Rollback operations should be in reverse order
        for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) {
            sqls.push_back((*it)->rollback_sql());
        }
        return sqls;
    }

    /// @brief Get migration name
    const std::string& name() const { return name_; }

    /// @brief Check if migration is empty
    bool empty() const { return operations_.empty(); }

    /// @brief Get number of operations
    size_t size() const { return operations_.size(); }
};

} // namespace relx::migrations 