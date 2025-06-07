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
TableMetadata extract_table_metadata(const Table& table_instance) {
  TableMetadata metadata;
  metadata.table_name = std::string(Table::table_name);

  // Use boost::pfr to iterate through all fields
  boost::pfr::for_each_field(table_instance, [&](const auto& field) {
    using field_type = std::remove_cvref_t<decltype(field)>;

    if constexpr (schema::is_column<field_type>) {
      // Extract column metadata
      ColumnMetadata col_meta;
      col_meta.name = std::string(field_type::name);
      col_meta.sql_definition = field.sql_definition();
      col_meta.sql_type = std::string(field_type::sql_type);
      col_meta.nullable = field_type::nullable;

      metadata.columns[col_meta.name] = std::move(col_meta);
    } else if constexpr (schema::is_constraint<field_type>) {
      // Extract constraint metadata
      ConstraintMetadata constraint_meta;
      constraint_meta.sql_definition = field.sql_definition();

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

  return metadata;
}

/// @brief Compare two table metadata structures and generate differences
/// @param old_metadata Metadata for the old table version
/// @param new_metadata Metadata for the new table version
/// @param options Migration options including column/constraint mappings
/// @return Migration containing the necessary operations
Migration diff_tables(const TableMetadata& old_metadata, const TableMetadata& new_metadata,
                      const MigrationOptions& options = {}) {
  Migration migration("diff_" + old_metadata.table_name + "_to_" + new_metadata.table_name);

  // Track which columns have been processed to handle renames
  std::unordered_set<std::string> processed_old_columns;
  std::unordered_set<std::string> processed_new_columns;

  // Find column differences

  // 1. Handle column renames first
  for (const auto& [old_name, new_name] : options.column_mappings) {
    auto old_it = old_metadata.columns.find(old_name);
    auto new_it = new_metadata.columns.find(new_name);

    if (old_it != old_metadata.columns.end() && new_it != new_metadata.columns.end()) {
      processed_old_columns.insert(old_name);
      processed_new_columns.insert(new_name);

      // Check if column definition also changed (rename + modify)
      // For renamed columns, we need to compare the essential properties excluding the name
      bool type_changed = (old_it->second.sql_type != new_it->second.sql_type);
      bool nullable_changed = (old_it->second.nullable != new_it->second.nullable);

      // Extract the core definition without the column name for comparison
      std::string old_core_def = old_it->second.sql_definition;
      std::string new_core_def = new_it->second.sql_definition;

      // Remove the old column name from old definition and new column name from new definition
      // to compare just the type and constraints
      auto old_name_pos = old_core_def.find(old_name);
      if (old_name_pos != std::string::npos) {
        old_core_def = old_core_def.substr(old_name_pos + old_name.length());
      }
      auto new_name_pos = new_core_def.find(new_name);
      if (new_name_pos != std::string::npos) {
        new_core_def = new_core_def.substr(new_name_pos + new_name.length());
      }

      if (type_changed || nullable_changed || old_core_def != new_core_def) {
        // Column is being renamed AND type/constraints changed
        // This requires data transformation, so we use: ADD new column, UPDATE data, DROP old
        // column This preserves data through transformation

        migration.add_operation<AddColumnOperation<ColumnMetadata>>(new_metadata.table_name,
                                                                    new_it->second);

        // Add UPDATE operation to transform data from old column to new column
        auto transform_it = options.column_transformations.find(old_name);
        if (transform_it != options.column_transformations.end()) {
          const auto& [forward_sql, backward_sql] = transform_it->second;
          migration.add_operation<UpdateDataOperation>(new_metadata.table_name, new_name, old_name,
                                                       forward_sql, backward_sql);
        } else {
          // No transformation provided - this will require manual intervention
          // We could add a comment or warning here in a production system
        }

        migration.add_operation<DropColumnOperation<ColumnMetadata>>(old_metadata.table_name,
                                                                     old_it->second);
      } else {
        // Column is only being renamed (no type/constraint changes)
        // Safe to use RENAME COLUMN operation
        migration.add_operation<RenameColumnOperation>(old_metadata.table_name, old_name, new_name);
      }
    }
  }

  // 2. Find new columns (in new but not in old, excluding renames)
  for (const auto& [col_name, col_meta] : new_metadata.columns) {
    if (processed_new_columns.find(col_name) == processed_new_columns.end() &&
        old_metadata.columns.find(col_name) == old_metadata.columns.end()) {
      // Column is genuinely new - need to add it
      migration.add_operation<AddColumnOperation<ColumnMetadata>>(new_metadata.table_name,
                                                                  col_meta);
    }
  }

  // 3. Find dropped columns (in old but not in new, excluding renames)
  for (const auto& [col_name, col_meta] : old_metadata.columns) {
    if (processed_old_columns.find(col_name) == processed_old_columns.end() &&
        new_metadata.columns.find(col_name) == new_metadata.columns.end()) {
      // Column was genuinely dropped
      migration.add_operation<DropColumnOperation<ColumnMetadata>>(old_metadata.table_name,
                                                                   col_meta);
    }
  }

  // 4. Find modified columns (same name but different definition, excluding renames)
  for (const auto& [col_name, new_col_meta] : new_metadata.columns) {
    if (processed_new_columns.find(col_name) == processed_new_columns.end()) {
      auto old_it = old_metadata.columns.find(col_name);
      if (old_it != old_metadata.columns.end() && old_it->second != new_col_meta) {
        // Column was modified - for simplicity, we'll drop and recreate
        migration.add_operation<DropColumnOperation<ColumnMetadata>>(old_metadata.table_name,
                                                                     old_it->second);
        migration.add_operation<AddColumnOperation<ColumnMetadata>>(new_metadata.table_name,
                                                                    new_col_meta);
      }
    }
  }

  // Find constraint differences

  // 1. Find new constraints (in new but not in old)
  for (const auto& [constraint_name, constraint_meta] : new_metadata.constraints) {
    if (old_metadata.constraints.find(constraint_name) == old_metadata.constraints.end()) {
      // New constraint - add it
      migration.add_operation<AddConstraintOperation>(new_metadata.table_name, constraint_meta);
    }
  }

  // 2. Find dropped constraints (in old but not in new)
  for (const auto& [constraint_name, constraint_meta] : old_metadata.constraints) {
    if (new_metadata.constraints.find(constraint_name) == new_metadata.constraints.end()) {
      // Constraint was dropped
      migration.add_operation<DropConstraintOperation>(old_metadata.table_name, constraint_meta);
    }
  }

  // 3. Find modified constraints (same name but different definition)
  for (const auto& [constraint_name, new_constraint_meta] : new_metadata.constraints) {
    auto old_it = old_metadata.constraints.find(constraint_name);
    if (old_it != old_metadata.constraints.end() && old_it->second != new_constraint_meta) {
      // Constraint was modified - drop old and add new
      migration.add_operation<DropConstraintOperation>(old_metadata.table_name, old_it->second);
      migration.add_operation<AddConstraintOperation>(new_metadata.table_name, new_constraint_meta);
    }
  }

  return migration;
}

/// @brief Generate migration from old table to new table
/// @tparam OldTable The old table type
/// @tparam NewTable The new table type
/// @param old_table Instance of the old table
/// @param new_table Instance of the new table
/// @param options Migration options including column/constraint mappings
/// @return Migration containing the necessary operations
template <schema::TableConcept OldTable, schema::TableConcept NewTable>
Migration generate_migration(const OldTable& old_table, const NewTable& new_table,
                             const MigrationOptions& options = {}) {
  static_assert(std::string_view(OldTable::table_name) == std::string_view(NewTable::table_name),
                "Table names must match for migration generation");

  auto old_metadata = extract_table_metadata(old_table);
  auto new_metadata = extract_table_metadata(new_table);

  return diff_tables(old_metadata, new_metadata, options);
}

/// @brief Generate migration to create a new table
/// @tparam Table The table type
/// @param table Instance of the table to create
/// @return Migration to create the table
template <schema::TableConcept Table>
Migration generate_create_table_migration(const Table& table) {
  Migration migration("create_" + std::string(Table::table_name));
  migration.add_operation<CreateTableOperation<Table>>(table);
  return migration;
}

/// @brief Generate migration to drop a table
/// @tparam Table The table type
/// @param table Instance of the table to drop
/// @return Migration to drop the table
template <schema::TableConcept Table>
Migration generate_drop_table_migration(const Table& table) {
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

  std::string to_sql() const override {
    return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition + ";";
  }

  std::string rollback_sql() const override {
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

  std::string to_sql() const override {
    return "ALTER TABLE " + table_name_ + " DROP COLUMN " + column_.name + ";";
  }

  std::string rollback_sql() const override {
    return "ALTER TABLE " + table_name_ + " ADD COLUMN " + column_.sql_definition + ";";
  }

  OperationType type() const override { return OperationType::DROP_COLUMN; }
};

}  // namespace relx::migrations