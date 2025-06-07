#pragma once

#include "core.hpp"
#include "diff.hpp"

#include <string>

namespace relx::migrations {

/// @brief ADD CONSTRAINT migration operation
class AddConstraintOperation : public MigrationOperation {
private:
    std::string table_name_;
    ConstraintMetadata constraint_;

public:
    AddConstraintOperation(std::string table_name, const ConstraintMetadata& constraint) 
        : table_name_(std::move(table_name)), constraint_(constraint) {}

    std::string to_sql() const override {
        if (constraint_.type == "INDEX") {
            // Indexes use CREATE INDEX syntax, not ALTER TABLE
            return "CREATE " + constraint_.sql_definition + ";";
        } else {
            return "ALTER TABLE " + table_name_ + " ADD " + constraint_.sql_definition + ";";
        }
    }

    std::string rollback_sql() const override {
        if (constraint_.type == "INDEX") {
            return "DROP INDEX IF EXISTS " + constraint_.name + ";";
        } else if (constraint_.type == "PRIMARY_KEY") {
            return "ALTER TABLE " + table_name_ + " DROP PRIMARY KEY;";
        } else {
            return "ALTER TABLE " + table_name_ + " DROP CONSTRAINT " + constraint_.name + ";";
        }
    }

    OperationType type() const override {
        return constraint_.type == "INDEX" ? OperationType::ADD_INDEX : OperationType::ADD_CONSTRAINT;
    }
};

/// @brief DROP CONSTRAINT migration operation
class DropConstraintOperation : public MigrationOperation {
private:
    std::string table_name_;
    ConstraintMetadata constraint_;

public:
    DropConstraintOperation(std::string table_name, const ConstraintMetadata& constraint) 
        : table_name_(std::move(table_name)), constraint_(constraint) {}

    std::string to_sql() const override {
        if (constraint_.type == "INDEX") {
            return "DROP INDEX IF EXISTS " + constraint_.name + ";";
        } else if (constraint_.type == "PRIMARY_KEY") {
            return "ALTER TABLE " + table_name_ + " DROP PRIMARY KEY;";
        } else {
            return "ALTER TABLE " + table_name_ + " DROP CONSTRAINT " + constraint_.name + ";";
        }
    }

    std::string rollback_sql() const override {
        if (constraint_.type == "INDEX") {
            return "CREATE " + constraint_.sql_definition + ";";
        } else {
            return "ALTER TABLE " + table_name_ + " ADD " + constraint_.sql_definition + ";";
        }
    }

    OperationType type() const override {
        return constraint_.type == "INDEX" ? OperationType::DROP_INDEX : OperationType::DROP_CONSTRAINT;
    }
};

/// @brief MODIFY COLUMN migration operation
class ModifyColumnOperation : public MigrationOperation {
private:
    std::string table_name_;
    ColumnMetadata old_column_;
    ColumnMetadata new_column_;

public:
    ModifyColumnOperation(std::string table_name, const ColumnMetadata& old_column, const ColumnMetadata& new_column) 
        : table_name_(std::move(table_name)), old_column_(old_column), new_column_(new_column) {}

    std::string to_sql() const override {
        // PostgreSQL syntax - other databases may vary
        return "ALTER TABLE " + table_name_ + " ALTER COLUMN " + new_column_.name + " TYPE " + new_column_.sql_type + ";";
    }

    std::string rollback_sql() const override {
        return "ALTER TABLE " + table_name_ + " ALTER COLUMN " + old_column_.name + " TYPE " + old_column_.sql_type + ";";
    }

    OperationType type() const override {
        return OperationType::MODIFY_COLUMN;
    }
};

} // namespace relx::migrations 