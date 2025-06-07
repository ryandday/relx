#pragma once

#include "../schema/check_constraint.hpp"
#include "../schema/column.hpp"
#include "../schema/foreign_key.hpp"
#include "../schema/index.hpp"
#include "../schema/primary_key.hpp"
#include "../schema/table.hpp"
#include "../schema/unique_constraint.hpp"
#include "core.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/pfr.hpp>

namespace relx::migrations {

// Forward declarations
class AddConstraintOperation;
class DropConstraintOperation;

/// @brief Options for controlling migration generation
struct MigrationOptions {
  /// @brief Map of old column name to new column name for renames
  std::unordered_map<std::string, std::string> column_mappings;

  /// @brief Map of old constraint name to new constraint name for renames
  std::unordered_map<std::string, std::string> constraint_mappings;

  /// @brief Whether to preserve data during column type changes (default: true)
  bool preserve_data = true;

  /// @brief Bidirectional SQL transformations for complex column changes
  /// Key: old column name, Value: {forward_sql, backward_sql}
  /// Forward: transforms old column data to new column format
  /// Backward: transforms new column data back to old column format (for rollback)
  std::unordered_map<std::string, std::pair<std::string, std::string>> column_transformations;
};

/// @brief Metadata about a column extracted from PFR analysis
struct ColumnMetadata {
  std::string name;
  std::string sql_definition;
  std::string sql_type;
  bool nullable;

  bool operator==(const ColumnMetadata& other) const {
    return name == other.name && sql_definition == other.sql_definition;
  }

  bool operator!=(const ColumnMetadata& other) const { return !(*this == other); }
};

/// @brief Metadata about a constraint extracted from PFR analysis
struct ConstraintMetadata {
  std::string name;
  std::string sql_definition;
  std::string type;  // "PRIMARY_KEY", "FOREIGN_KEY", "UNIQUE", "CHECK", "INDEX"

  bool operator==(const ConstraintMetadata& other) const {
    return name == other.name && sql_definition == other.sql_definition && type == other.type;
  }

  bool operator!=(const ConstraintMetadata& other) const { return !(*this == other); }
};

/// @brief Complete metadata about a table
struct TableMetadata {
  std::string table_name;
  std::unordered_map<std::string, ColumnMetadata> columns;
  std::unordered_map<std::string, ConstraintMetadata> constraints;
};

/// @brief Extract table metadata using Boost.PFR
/// @tparam Table The table type
/// @param table_instance Instance of the table
/// @return Metadata about the table structure
template <schema::TableConcept Table>
MigrationResult<TableMetadata> extract_table_metadata(const Table& table_instance) {
  try {
    TableMetadata metadata;
    metadata.table_name = std::string(Table::table_name);

    // Track any errors that occur during field processing
    std::optional<MigrationError> error;

    // Use boost::pfr to iterate through all fields
    boost::pfr::for_each_field(table_instance, [&](const auto& field) {
      using field_type = std::remove_cvref_t<decltype(field)>;

      // Skip processing if we already have an error
      if (error.has_value()) {
        return;
      }

      if constexpr (schema::is_column<field_type>) {
        // Extract column metadata
        ColumnMetadata col_meta;
        col_meta.name = std::string(field_type::name);

        try {
          col_meta.sql_definition = field.sql_definition();
        } catch (const std::exception& e) {
          error = MigrationError::make(MigrationErrorType::MIGRATION_GENERATION_FAILED,
                                       "Failed to get SQL definition for column '" + col_meta.name +
                                           "': " + e.what(),
                                       std::string(Table::table_name));
          return;
        }

        col_meta.sql_type = std::string(field_type::sql_type);
        col_meta.nullable = field_type::nullable;

        metadata.columns[col_meta.name] = std::move(col_meta);
      } else if constexpr (schema::is_constraint<field_type>) {
        // Extract constraint metadata
        ConstraintMetadata constraint_meta;

        try {
          constraint_meta.sql_definition = field.sql_definition();
        } catch (const std::exception& e) {
          error = MigrationError::make(MigrationErrorType::MIGRATION_GENERATION_FAILED,
                                       "Failed to get SQL definition for constraint: " +
                                           std::string(e.what()),
                                       std::string(Table::table_name));
          return;
        }

        // Determine constraint type and generate a unique name based on SQL definition patterns
        std::string sql_def = constraint_meta.sql_definition;
        if (sql_def.find("PRIMARY KEY") != std::string::npos) {
          constraint_meta.type = "PRIMARY_KEY";
          constraint_meta.name = metadata.table_name + "_pk";
        } else if (sql_def.find("FOREIGN KEY") != std::string::npos ||
                   sql_def.find("REFERENCES") != std::string::npos) {
          constraint_meta.type = "FOREIGN_KEY";
          constraint_meta.name = metadata.table_name + "_fk_" +
                                 std::to_string(metadata.constraints.size());
        } else if (sql_def.find("UNIQUE") != std::string::npos) {
          constraint_meta.type = "UNIQUE";
          constraint_meta.name = metadata.table_name + "_unique_" +
                                 std::to_string(metadata.constraints.size());
        } else if (sql_def.find("CHECK") != std::string::npos) {
          constraint_meta.type = "CHECK";
          constraint_meta.name = metadata.table_name + "_check_" +
                                 std::to_string(metadata.constraints.size());
        } else if (sql_def.find("INDEX") != std::string::npos) {
          constraint_meta.type = "INDEX";
          constraint_meta.name = metadata.table_name + "_idx_" +
                                 std::to_string(metadata.constraints.size());
        } else {
          constraint_meta.type = "UNKNOWN";
          constraint_meta.name = metadata.table_name + "_constraint_" +
                                 std::to_string(metadata.constraints.size());
        }

        metadata.constraints[constraint_meta.name] = std::move(constraint_meta);
      }
    });

    // Check if any errors occurred during field processing
    if (error.has_value()) {
      return std::unexpected(*error);
    }

    return metadata;
  } catch (const std::exception& e) {
    return std::unexpected(
        MigrationError::make(MigrationErrorType::MIGRATION_GENERATION_FAILED,
                             "Failed to extract table metadata: " + std::string(e.what()),
                             std::string(Table::table_name)));
  }
}

/// @brief Generate migration from table metadata differences
/// @param old_metadata Metadata for the old table version
/// @param new_metadata Metadata for the new table version
/// @param options Migration options including column/constraint mappings
/// @return Migration containing the necessary operations
MigrationResult<Migration> diff_tables(const TableMetadata& old_metadata,
                                       const TableMetadata& new_metadata,
                                       const MigrationOptions& options = {});

/// @brief Generate migration from old table to new table
/// @tparam OldTable The old table type
/// @tparam NewTable The new table type
/// @param old_table Instance of the old table
/// @param new_table Instance of the new table
/// @param options Migration options including column/constraint mappings
/// @return Migration containing the necessary operations
template <schema::TableConcept OldTable, schema::TableConcept NewTable>
MigrationResult<Migration> generate_migration(const OldTable& old_table, const NewTable& new_table,
                                              const MigrationOptions& options = {}) {
  static_assert(std::string_view(OldTable::table_name) == std::string_view(NewTable::table_name),
                "Table names must match for migration generation");

  auto old_metadata_result = extract_table_metadata(old_table);
  if (!old_metadata_result) {
    return std::unexpected(old_metadata_result.error());
  }

  auto new_metadata_result = extract_table_metadata(new_table);
  if (!new_metadata_result) {
    return std::unexpected(new_metadata_result.error());
  }

  return diff_tables(*old_metadata_result, *new_metadata_result, options);
}

/// @brief Generate migration to create a new table
/// @tparam Table The table type
/// @param table Instance of the table to create
/// @return Migration to create the table
template <schema::TableConcept Table>
MigrationResult<Migration> generate_create_table_migration(const Table& table) {
  Migration migration("create_" + std::string(Table::table_name));
  migration.add_operation<CreateTableOperation<Table>>(table);
  return migration;
}

/// @brief Generate migration to drop a table
/// @tparam Table The table type
/// @param table Instance of the table to drop
/// @return Migration to drop the table
template <schema::TableConcept Table>
MigrationResult<Migration> generate_drop_table_migration(const Table& table) {
  Migration migration("drop_" + std::string(Table::table_name));
  migration.add_operation<DropTableOperation<Table>>(table);
  return migration;
}

/// @brief Specialized AddColumnOperation for ColumnMetadata
template <>
class AddColumnOperation<ColumnMetadata> : public MigrationOperation {
private:
  std::string table_name_;
  ColumnMetadata column_;

public:
  AddColumnOperation(std::string table_name, const ColumnMetadata& column)
      : table_name_(std::move(table_name)), column_(column) {}

  MigrationResult<std::string> to_sql() const override {
    if (column_.sql_definition.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column SQL definition cannot be empty",
                                                  table_name_ + "." + column_.name));
    }
    return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (column_.name.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column name cannot be empty", table_name_));
    }
    return "ALTER TABLE " + table_name_ + " DROP COLUMN " + column_.name + ";";
  }

  OperationType type() const override { return OperationType::ADD_COLUMN; }
};

/// @brief Specialized DropColumnOperation for ColumnMetadata
template <>
class DropColumnOperation<ColumnMetadata> : public MigrationOperation {
private:
  std::string table_name_;
  ColumnMetadata column_;

public:
  DropColumnOperation(std::string table_name, const ColumnMetadata& column)
      : table_name_(std::move(table_name)), column_(column) {}

  MigrationResult<std::string> to_sql() const override {
    if (column_.name.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column name cannot be empty", table_name_));
    }
    return "ALTER TABLE " + table_name_ + " DROP COLUMN " + column_.name + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (column_.sql_definition.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column SQL definition cannot be empty",
                                                  table_name_ + "." + column_.name));
    }
    return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition + ";";
  }

  OperationType type() const override { return OperationType::DROP_COLUMN; }
};

/// @brief Extract table metadata from a table instance
/// @tparam Table The table type
/// @param table Instance of the table
/// @return Table metadata for migration diffing
template <schema::TableConcept Table>
MigrationResult<TableMetadata> extract_table_metadata(const Table& table);

}  // namespace relx::migrations