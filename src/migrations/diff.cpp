#include <unordered_set>

#include <relx/migrations/constraint_operations.hpp>
#include <relx/migrations/diff.hpp>

namespace relx::migrations {

MigrationResult<Migration> diff_tables(const TableMetadata& old_metadata,
                                       const TableMetadata& new_metadata,
                                       const MigrationOptions& options) {
  // Validate table names match for migration generation
  if (old_metadata.table_name != new_metadata.table_name) {
    return std::unexpected(MigrationError::make(
        MigrationErrorType::VALIDATION_FAILED, "Table names must match for migration generation",
        old_metadata.table_name + " vs " + new_metadata.table_name));
  }

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
    } else {
      // Column mapping references non-existent columns
      if (old_it == old_metadata.columns.end()) {
        return std::unexpected(
            MigrationError::make(MigrationErrorType::COLUMN_NOT_FOUND,
                                 "Column '" + old_name + "' not found in old table definition",
                                 old_metadata.table_name));
      }
      if (new_it == new_metadata.columns.end()) {
        return std::unexpected(
            MigrationError::make(MigrationErrorType::COLUMN_NOT_FOUND,
                                 "Column '" + new_name + "' not found in new table definition",
                                 new_metadata.table_name));
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

}  // namespace relx::migrations