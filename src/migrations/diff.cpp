#include <algorithm>
#include <cctype>
#include <unordered_set>

#include <relx/migrations/constraint_operations.hpp>
#include <relx/migrations/diff.hpp>

namespace relx::migrations {

namespace {

/// @brief Whether a constraint definition references a column as a whole identifier
/// (bare or quoted). Used to find constraints that PostgreSQL would cascade away when
/// the column is dropped and recreated.
bool mentions_column(const std::string& definition, const std::string& column_name) {
  if (definition.find("\"" + column_name + "\"") != std::string::npos) {
    return true;
  }
  const auto is_ident_char = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
  };
  std::size_t pos = 0;
  while ((pos = definition.find(column_name, pos)) != std::string::npos) {
    const std::size_t end = pos + column_name.size();
    const bool left_ok = pos == 0 || !is_ident_char(definition[pos - 1]);
    const bool right_ok = end >= definition.size() || !is_ident_char(definition[end]);
    if (left_ok && right_ok) {
      return true;
    }
    pos = end;
  }
  return false;
}

}  // namespace

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

  // Pass 1: resolve renames (validating the mappings), and classify every column
  // change so constraint operations can be ordered around the column operations:
  // constraints must drop BEFORE columns change (dropping an FK-bearing column first
  // would let PostgreSQL cascade the constraint away, making a later explicit DROP
  // CONSTRAINT fail) and add AFTER (an ADD CONSTRAINT cannot precede its column).
  // Migration::rollback_sql reverses the operation list, so this ordering also makes
  // rollbacks valid.

  struct RenameOnly {
    std::string old_name;
    std::string new_name;
  };
  struct RenameAndTransform {
    const ColumnMetadata* old_column;
    const ColumnMetadata* new_column;
    std::string forward_sql;
    std::string backward_sql;
  };
  std::vector<RenameOnly> renames;
  std::vector<RenameAndTransform> rename_transforms;

  for (const auto& [old_name, new_name] : options.column_mappings) {
    auto old_it = old_metadata.columns.find(old_name);
    auto new_it = new_metadata.columns.find(new_name);

    if (old_it == old_metadata.columns.end()) {
      return std::unexpected(MigrationError::make(
          MigrationErrorType::COLUMN_NOT_FOUND,
          "Column '" + old_name + "' not found in old table definition", old_metadata.table_name));
    }
    if (new_it == new_metadata.columns.end()) {
      return std::unexpected(MigrationError::make(
          MigrationErrorType::COLUMN_NOT_FOUND,
          "Column '" + new_name + "' not found in new table definition", new_metadata.table_name));
    }

    processed_old_columns.insert(old_name);
    processed_new_columns.insert(new_name);

    // Check if the column definition also changed (rename + modify). Compare the
    // core definition with the column name removed.
    const bool type_changed = (old_it->second.sql_type != new_it->second.sql_type);
    const bool nullable_changed = (old_it->second.nullable != new_it->second.nullable);

    std::string old_core_def = old_it->second.sql_definition;
    std::string new_core_def = new_it->second.sql_definition;
    auto old_name_pos = old_core_def.find(old_name);
    if (old_name_pos != std::string::npos) {
      old_core_def = old_core_def.substr(old_name_pos + old_name.length());
    }
    auto new_name_pos = new_core_def.find(new_name);
    if (new_name_pos != std::string::npos) {
      new_core_def = new_core_def.substr(new_name_pos + new_name.length());
    }

    if (type_changed || nullable_changed || old_core_def != new_core_def) {
      // Rename + type/constraint change: expressed as ADD new column, UPDATE data,
      // DROP old column. Without a data transformation this silently loses every
      // row's value, so the transformation is required, not optional.
      auto transform_it = options.column_transformations.find(old_name);
      if (transform_it == options.column_transformations.end()) {
        return std::unexpected(MigrationError::make(
            MigrationErrorType::VALIDATION_FAILED,
            "Renaming '" + old_name + "' to '" + new_name +
                "' also changes its definition; provide a column_transformations entry for '" +
                old_name + "' (forward and backward SQL) so the data survives the move",
            old_metadata.table_name));
      }
      rename_transforms.push_back({&old_it->second, &new_it->second, transform_it->second.first,
                                   transform_it->second.second});
    } else {
      renames.push_back({old_name, new_name});
    }
  }

  // Classify remaining column changes; rebuilt columns (drop + recreate) need their
  // untouched constraints dropped and re-added around the rebuild
  std::vector<const ColumnMetadata*> new_columns;
  std::vector<const ColumnMetadata*> dropped_columns;
  struct ModifiedColumn {
    const ColumnMetadata* old_column;
    const ColumnMetadata* new_column;
    bool in_place;
  };
  std::vector<ModifiedColumn> modified_columns;
  std::unordered_set<std::string> rebuilt_columns;
  struct EnumAddition {
    std::string forward_sql;
    std::string rollback_note;
  };
  std::vector<EnumAddition> enum_additions;

  for (const auto& [col_name, col_meta] : new_metadata.columns) {
    if (processed_new_columns.find(col_name) == processed_new_columns.end() &&
        old_metadata.columns.find(col_name) == old_metadata.columns.end()) {
      new_columns.push_back(&col_meta);
    }
  }
  for (const auto& [col_name, col_meta] : old_metadata.columns) {
    if (processed_old_columns.find(col_name) == processed_old_columns.end() &&
        new_metadata.columns.find(col_name) == new_metadata.columns.end()) {
      dropped_columns.push_back(&col_meta);
    }
  }
  for (const auto& [col_name, new_col_meta] : new_metadata.columns) {
    if (processed_new_columns.find(col_name) != processed_new_columns.end()) {
      continue;
    }
    auto old_it = old_metadata.columns.find(col_name);
    if (old_it == old_metadata.columns.end()) {
      continue;
    }

    // Native enum value changes are ALTER TYPE, not column changes: without this the
    // schema change would diff to nothing and silently never reach the database
    if (!new_col_meta.enum_type_name.empty() &&
        new_col_meta.enum_type_name == old_it->second.enum_type_name &&
        new_col_meta.enum_values != old_it->second.enum_values) {
      for (const auto& old_value : old_it->second.enum_values) {
        if (std::find(new_col_meta.enum_values.begin(), new_col_meta.enum_values.end(),
                      old_value) == new_col_meta.enum_values.end()) {
          return std::unexpected(MigrationError::make(
              MigrationErrorType::UNSUPPORTED_OPERATION,
              "PostgreSQL cannot remove value '" + old_value + "' from enum type '" +
                  new_col_meta.enum_type_name +
                  "'; recreate the type (and dependent columns) explicitly instead",
              old_metadata.table_name + "." + col_name));
        }
      }
      for (const auto& new_value : new_col_meta.enum_values) {
        if (std::find(old_it->second.enum_values.begin(), old_it->second.enum_values.end(),
                      new_value) == old_it->second.enum_values.end()) {
          enum_additions.push_back(
              {"ALTER TYPE " + new_col_meta.enum_type_name + " ADD VALUE '" + new_value + "';",
               "-- PostgreSQL cannot remove enum value '" + new_value + "' from type '" +
                   new_col_meta.enum_type_name + "'; it stays in place"});
        }
      }
    }

    if (old_it->second == new_col_meta) {
      continue;
    }
    const bool in_place = options.preserve_data &&
                          ModifyColumnOperation::can_express(old_it->second, new_col_meta);
    modified_columns.push_back({&old_it->second, &new_col_meta, in_place});
    if (!in_place) {
      rebuilt_columns.insert(col_name);
    }
  }

  // A constraint is affected by a rebuild when its definition references a rebuilt
  // column: PostgreSQL cascades it away at DROP COLUMN, so it must be explicitly
  // dropped first and re-added after - otherwise it silently vanishes
  const auto affected_by_rebuild = [&](const ConstraintMetadata& constraint) {
    for (const auto& col_name : rebuilt_columns) {
      if (mentions_column(constraint.sql_definition, col_name)) {
        return true;
      }
    }
    return false;
  };

  // Phase 1: constraint drops (dropped, modified-old, and rebuild-affected)
  for (const auto& [constraint_name, constraint_meta] : old_metadata.constraints) {
    auto new_it = new_metadata.constraints.find(constraint_name);
    const bool dropped = new_it == new_metadata.constraints.end();
    const bool modified = !dropped && new_it->second != constraint_meta;
    const bool rebuild_hit = !dropped && !modified && affected_by_rebuild(constraint_meta);
    if (dropped || modified || rebuild_hit) {
      migration.add_operation<DropConstraintOperation>(old_metadata.table_name, constraint_meta);
    }
  }

  // Phase 2: column operations (enum type extensions first - a new enum value must
  // exist before any data operation could reference it)
  for (const auto& addition : enum_additions) {
    migration.add_operation<RawSqlOperation>(addition.forward_sql, addition.rollback_note,
                                             OperationType::MODIFY_COLUMN);
  }
  for (const auto& rename : renames) {
    migration.add_operation<RenameColumnOperation>(old_metadata.table_name, rename.old_name,
                                                   rename.new_name);
  }
  for (const auto& transform : rename_transforms) {
    migration.add_operation<AddColumnOperation<ColumnMetadata>>(new_metadata.table_name,
                                                                *transform.new_column);
    migration.add_operation<UpdateDataOperation>(
        new_metadata.table_name, transform.new_column->name, transform.old_column->name,
        transform.forward_sql, transform.backward_sql);
    migration.add_operation<DropColumnOperation<ColumnMetadata>>(old_metadata.table_name,
                                                                 *transform.old_column);
  }
  for (const auto* col_meta : new_columns) {
    if (!col_meta->nullable && col_meta->sql_definition.find(" DEFAULT ") == std::string::npos) {
      migration.add_warning("Column '" + col_meta->name +
                            "' is added NOT NULL without a "
                            "DEFAULT: this fails on populated tables. Add a default or make "
                            "the column nullable and backfill.");
    }
    migration.add_operation<AddColumnOperation<ColumnMetadata>>(new_metadata.table_name, *col_meta);
  }
  for (const auto* col_meta : dropped_columns) {
    migration.add_operation<DropColumnOperation<ColumnMetadata>>(old_metadata.table_name,
                                                                 *col_meta);
  }
  for (const auto& modified : modified_columns) {
    if (modified.in_place) {
      // In-place ALTER COLUMN (TYPE ... USING / SET NOT NULL / SET DEFAULT) keeps
      // the column's data; a caller-provided transformation refines the USING cast
      auto transform_it = options.column_transformations.find(modified.new_column->name);
      if (transform_it != options.column_transformations.end()) {
        migration.add_operation<ModifyColumnOperation>(
            new_metadata.table_name, *modified.old_column, *modified.new_column,
            transform_it->second.first, transform_it->second.second);
      } else {
        migration.add_operation<ModifyColumnOperation>(new_metadata.table_name,
                                                       *modified.old_column, *modified.new_column);
      }
    } else {
      // Change is not expressible in place (or data preservation was waived):
      // drop and recreate, losing the column's data
      migration.add_operation<DropColumnOperation<ColumnMetadata>>(old_metadata.table_name,
                                                                   *modified.old_column);
      migration.add_operation<AddColumnOperation<ColumnMetadata>>(new_metadata.table_name,
                                                                  *modified.new_column);
    }
  }

  // Phase 3: constraint adds (new, modified-new, and rebuild-affected re-adds)
  for (const auto& [constraint_name, constraint_meta] : new_metadata.constraints) {
    auto old_it = old_metadata.constraints.find(constraint_name);
    const bool added = old_it == old_metadata.constraints.end();
    const bool modified = !added && old_it->second != constraint_meta;
    const bool rebuild_hit = !added && !modified && affected_by_rebuild(constraint_meta);
    if (added || modified || rebuild_hit) {
      migration.add_operation<AddConstraintOperation>(new_metadata.table_name, constraint_meta);
    }
  }

  return migration;
}

}  // namespace relx::migrations
