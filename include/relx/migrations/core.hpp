#pragma once

#include "../schema/column.hpp"
#include "../schema/core.hpp"
#include "../schema/fixed_string.hpp"
#include "../schema/table.hpp"

#include <expected>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <boost/pfr.hpp>

namespace relx::migrations {

/// @brief Error types for migration operations
enum class MigrationErrorType {
  INVALID_TABLE_STRUCTURE,
  UNSUPPORTED_OPERATION,
  COLUMN_NOT_FOUND,
  CONSTRAINT_NOT_FOUND,
  INCOMPATIBLE_TYPES,
  MIGRATION_GENERATION_FAILED,
  SQL_GENERATION_FAILED,
  VALIDATION_FAILED
};

/// @brief Error information for migration operations
struct MigrationError {
  MigrationErrorType type;
  std::string message;
  std::string context;  ///< Additional context (table name, column name, etc.)
  
  /// @brief Create a MigrationError with automatic formatting
  static MigrationError make(MigrationErrorType type, const std::string& message, 
                            const std::string& context = "") {
    return MigrationError{type, message, context};
  }
  
  /// @brief Get a formatted error message
  std::string format() const {
    if (context.empty()) {
      return message;
    }
    return context + ": " + message;
  }
};

/// @brief Result type for migration operations
template <typename T>
using MigrationResult = std::expected<T, MigrationError>;

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
  virtual MigrationResult<std::string> to_sql() const = 0;
  virtual MigrationResult<std::string> rollback_sql() const = 0;
  virtual OperationType type() const = 0;
  virtual MigrationResult<std::vector<std::string>> bind_params() const { 
    return std::vector<std::string>{}; 
  }
  virtual MigrationResult<std::vector<std::string>> rollback_bind_params() const { 
    return std::vector<std::string>{}; 
  }
};

/// @brief CREATE TABLE migration operation
template <schema::TableConcept Table>
class CreateTableOperation : public MigrationOperation {
private:
  const Table& table_;

public:
  explicit CreateTableOperation(const Table& table) : table_(table) {}

  MigrationResult<std::string> to_sql() const override { 
    try {
      return schema::create_table(table_).to_sql();
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate CREATE TABLE SQL: " + std::string(e.what()),
        std::string(Table::table_name)
      ));
    }
  }

  MigrationResult<std::string> rollback_sql() const override {
    try {
      return schema::drop_table(table_).if_exists().to_sql();
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate DROP TABLE SQL: " + std::string(e.what()),
        std::string(Table::table_name)
      ));
    }
  }

  OperationType type() const override { return OperationType::CREATE_TABLE; }
};

/// @brief DROP TABLE migration operation
template <schema::TableConcept Table>
class DropTableOperation : public MigrationOperation {
private:
  const Table& table_;

public:
  explicit DropTableOperation(const Table& table) : table_(table) {}

  MigrationResult<std::string> to_sql() const override { 
    try {
      return schema::drop_table(table_).if_exists().to_sql();
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate DROP TABLE SQL: " + std::string(e.what()),
        std::string(Table::table_name)
      ));
    }
  }

  MigrationResult<std::string> rollback_sql() const override {
    try {
      return schema::create_table(table_).to_sql();
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate CREATE TABLE SQL: " + std::string(e.what()),
        std::string(Table::table_name)
      ));
    }
  }

  OperationType type() const override { return OperationType::DROP_TABLE; }
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

  MigrationResult<std::string> to_sql() const override {
    try {
      return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition() + ";";
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate ADD COLUMN SQL: " + std::string(e.what()),
        table_name_ + "." + std::string(Column::name)
      ));
    }
  }

  MigrationResult<std::string> rollback_sql() const override {
    try {
      return "ALTER TABLE " + table_name_ + " DROP COLUMN " + std::string(Column::name) + ";";
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate DROP COLUMN SQL: " + std::string(e.what()),
        table_name_ + "." + std::string(Column::name)
      ));
    }
  }

  OperationType type() const override { return OperationType::ADD_COLUMN; }
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

  MigrationResult<std::string> to_sql() const override {
    try {
      return "ALTER TABLE " + table_name_ + " DROP COLUMN " + std::string(Column::name) + ";";
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate DROP COLUMN SQL: " + std::string(e.what()),
        table_name_ + "." + std::string(Column::name)
      ));
    }
  }

  MigrationResult<std::string> rollback_sql() const override {
    try {
      return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition() + ";";
    } catch (const std::exception& e) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::SQL_GENERATION_FAILED,
        "Failed to generate ADD COLUMN SQL: " + std::string(e.what()),
        table_name_ + "." + std::string(Column::name)
      ));
    }
  }

  OperationType type() const override { return OperationType::DROP_COLUMN; }
};

/// @brief RENAME COLUMN migration operation
class RenameColumnOperation : public MigrationOperation {
private:
  std::string table_name_;
  std::string old_name_;
  std::string new_name_;

public:
  RenameColumnOperation(std::string table_name, std::string old_name, std::string new_name)
      : table_name_(std::move(table_name)), old_name_(std::move(old_name)),
        new_name_(std::move(new_name)) {}

  MigrationResult<std::string> to_sql() const override {
    if (old_name_.empty() || new_name_.empty()) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED,
        "Column names cannot be empty",
        table_name_ + "." + old_name_ + " -> " + new_name_
      ));
    }
    return "ALTER TABLE " + table_name_ + " RENAME COLUMN " + old_name_ + " TO " + new_name_ + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (old_name_.empty() || new_name_.empty()) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED,
        "Column names cannot be empty",
        table_name_ + "." + new_name_ + " -> " + old_name_
      ));
    }
    return "ALTER TABLE " + table_name_ + " RENAME COLUMN " + new_name_ + " TO " + old_name_ + ";";
  }

  OperationType type() const override { return OperationType::RENAME_COLUMN; }
};

/// @brief RENAME CONSTRAINT migration operation
class RenameConstraintOperation : public MigrationOperation {
private:
  std::string table_name_;
  std::string old_name_;
  std::string new_name_;

public:
  RenameConstraintOperation(std::string table_name, std::string old_name, std::string new_name)
      : table_name_(std::move(table_name)), old_name_(std::move(old_name)),
        new_name_(std::move(new_name)) {}

  MigrationResult<std::string> to_sql() const override {
    if (old_name_.empty() || new_name_.empty()) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED,
        "Constraint names cannot be empty",
        table_name_ + " constraint: " + old_name_ + " -> " + new_name_
      ));
    }
    return "ALTER TABLE " + table_name_ + " RENAME CONSTRAINT " + old_name_ + " TO " + new_name_ + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (old_name_.empty() || new_name_.empty()) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED,
        "Constraint names cannot be empty",
        table_name_ + " constraint: " + new_name_ + " -> " + old_name_
      ));
    }
    return "ALTER TABLE " + table_name_ + " RENAME CONSTRAINT " + new_name_ + " TO " + old_name_ + ";";
  }

  OperationType type() const override { return OperationType::RENAME_CONSTRAINT; }
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

  MigrationResult<std::string> to_sql() const override {
    if (forward_transform_.empty()) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED,
        "Forward transformation cannot be empty",
        table_name_ + "." + target_column_
      ));
    }
    return "UPDATE " + table_name_ + " SET " + target_column_ + " = " + forward_transform_ + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (backward_transform_.empty()) {
      return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED,
        "Backward transformation cannot be empty",
        table_name_ + "." + source_column_
      ));
    }
    return "UPDATE " + table_name_ + " SET " + source_column_ + " = " + backward_transform_ + ";";
  }

  OperationType type() const override { return OperationType::UPDATE_DATA; }
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
  MigrationResult<std::vector<std::string>> forward_sql() const {
    std::vector<std::string> sqls;
    sqls.reserve(operations_.size());
    
    for (const auto& op : operations_) {
      auto sql_result = op->to_sql();
      if (!sql_result) {
        return std::unexpected(sql_result.error());
      }
      sqls.push_back(*sql_result);
    }
    return sqls;
  }

  /// @brief Generate rollback migration SQL
  MigrationResult<std::vector<std::string>> rollback_sql() const {
    std::vector<std::string> sqls;
    sqls.reserve(operations_.size());
    
    // Rollback operations should be in reverse order
    for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) {
      auto sql_result = (*it)->rollback_sql();
      if (!sql_result) {
        return std::unexpected(sql_result.error());
      }
      sqls.push_back(*sql_result);
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

}  // namespace relx::migrations