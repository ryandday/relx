#pragma once

#include "../reflect.hpp"
#include "../schema/annotated_table.hpp"
#include "../schema/column.hpp"
#include "../schema/table.hpp"
#include "core.hpp"

#include <cstdint>
#include <map>
#include <meta>
#include <string>
#include <unordered_set>
#include <vector>

namespace relx::migrations {

// Forward declarations
class AddConstraintOperation;
class DropConstraintOperation;

/// @brief Options for controlling migration generation
/// @note Ordered maps so generated operations are deterministic across standard libraries
struct MigrationOptions {
  /// @brief Map of old column name to new column name for renames
  std::map<std::string, std::string> column_mappings;

  /// @brief Map of old constraint name to new constraint name for renames
  std::map<std::string, std::string> constraint_mappings;

  /// @brief Whether to preserve data during column type changes (default: true)
  bool preserve_data = true;

  /// @brief Bidirectional SQL transformations for complex column changes
  /// Key: old column name, Value: {forward_sql, backward_sql}
  /// Forward: transforms old column data to new column format
  /// Backward: transforms new column data back to old column format (for rollback)
  std::map<std::string, std::pair<std::string, std::string>> column_transformations;
};

namespace detail {

/// @brief Collapse whitespace runs to single spaces and trim, so cosmetic formatting
/// differences in generated DDL don't read as schema changes
inline std::string normalize_whitespace(std::string_view sql) {
  std::string out;
  out.reserve(sql.size());
  bool pending_space = false;
  for (const char c : sql) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      pending_space = !out.empty();
      continue;
    }
    if (pending_space) {
      out += ' ';
      pending_space = false;
    }
    out += c;
  }
  return out;
}

}  // namespace detail

/// @brief Metadata about a column extracted via reflection
struct ColumnMetadata {
  std::string name;
  std::string sql_definition;
  std::string sql_type;
  bool nullable;

  bool operator==(const ColumnMetadata& other) const {
    return name == other.name && sql_type == other.sql_type && nullable == other.nullable &&
           detail::normalize_whitespace(sql_definition) ==
               detail::normalize_whitespace(other.sql_definition);
  }

  bool operator!=(const ColumnMetadata& other) const { return !(*this == other); }
};

/// @brief Metadata about a constraint extracted via reflection
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
/// @note Ordered maps so diffing emits operations in a deterministic (alphabetical) order
struct TableMetadata {
  std::string table_name;
  std::map<std::string, ColumnMetadata> columns;
  std::map<std::string, ConstraintMetadata> constraints;
};

namespace detail {

/// @brief FNV-1a hash for content-derived constraint names (constraints with no
/// column list, e.g. CHECK expressions)
inline std::uint64_t fnv1a(std::string_view text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char c : text) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

inline std::string content_hash(std::string_view text) {
  constexpr char digits[] = "0123456789abcdef";
  std::uint64_t hash = fnv1a(normalize_whitespace(text));
  hash ^= hash >> 32;
  std::string out(8, '0');
  for (std::size_t i = 0; i < 8; ++i) {
    out[7 - i] = digits[hash & 0xF];
    hash >>= 4;
  }
  return out;
}

/// @brief The local column list of a constraint definition ("UNIQUE (a, b)" -> "a_b"),
/// sanitized for use in an identifier; empty when there is no parenthesized list
inline std::string constrained_columns_slug(std::string_view sql) {
  const std::size_t open = sql.find('(');
  if (open == std::string_view::npos) {
    return {};
  }
  const std::size_t close = sql.find(')', open);
  if (close == std::string_view::npos) {
    return {};
  }
  std::string slug;
  for (const char c : sql.substr(open + 1, close - open - 1)) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
      slug += c;
    } else if (c == ',') {
      slug += '_';
    }
  }
  return slug;
}

/// @brief Classify a constraint SQL definition and register it under a deterministic,
/// content-derived name. Names derive from the constrained columns (or a content hash),
/// never from insertion position, so reordering struct fields cannot produce phantom
/// drop/add pairs.
inline void add_constraint_metadata(TableMetadata& metadata, std::string sql_def) {
  ConstraintMetadata constraint_meta;
  constraint_meta.sql_definition = std::move(sql_def);

  const std::string& sql = constraint_meta.sql_definition;
  const auto content_name = [&sql](const std::string& prefix) {
    const std::string slug = constrained_columns_slug(sql);
    return slug.empty() ? prefix + content_hash(sql) : prefix + slug;
  };
  if (sql.find("PRIMARY KEY") != std::string::npos) {
    constraint_meta.type = "PRIMARY_KEY";
    constraint_meta.name = metadata.table_name + "_pk";
  } else if (sql.find("FOREIGN KEY") != std::string::npos ||
             sql.find("REFERENCES") != std::string::npos) {
    constraint_meta.type = "FOREIGN_KEY";
    constraint_meta.name = content_name(metadata.table_name + "_fk_");
  } else if (sql.find("UNIQUE") != std::string::npos) {
    constraint_meta.type = "UNIQUE";
    constraint_meta.name = content_name(metadata.table_name + "_unique_");
  } else if (sql.find("CHECK") != std::string::npos) {
    constraint_meta.type = "CHECK";
    constraint_meta.name = metadata.table_name + "_check_" + content_hash(sql);
  } else if (sql.find("INDEX") != std::string::npos) {
    constraint_meta.type = "INDEX";
    constraint_meta.name = content_name(metadata.table_name + "_idx_");
  } else {
    constraint_meta.type = "UNKNOWN";
    constraint_meta.name = metadata.table_name + "_constraint_" + content_hash(sql);
  }

  // An explicit CONSTRAINT name overrides the generated positional name, so
  // ADD/DROP CONSTRAINT operations target the name that actually exists in the DB
  constexpr std::string_view name_prefix = "CONSTRAINT ";
  if (sql.starts_with(name_prefix)) {
    const std::size_t name_end = sql.find(' ', name_prefix.size());
    if (name_end != std::string::npos) {
      constraint_meta.name = sql.substr(name_prefix.size(), name_end - name_prefix.size());
    }
  }

  metadata.constraints[constraint_meta.name] = std::move(constraint_meta);
}

// clang-format off

/// @brief Add the table-level annotation constraints and indexes of an annotated
/// struct T to the metadata. Cross-column constraints only exist as struct
/// annotations, so without this walk a migration diff would silently drop them.
template <typename T>
void add_annotation_constraint_metadata(TableMetadata& metadata) {
  template for (constexpr std::meta::info a :
                std::define_static_array(std::meta::annotations_of(^^T))) {
    using A = typename [:std::meta::remove_cv(std::meta::type_of(a)):];
    if constexpr (schema::detail::TableConstraintAnnotation<A>) {
      constexpr std::string_view sql =
          std::define_static_string(std::meta::extract<A>(a).constraint_sql());
      add_constraint_metadata(metadata, std::string(sql));
    } else if constexpr (schema::detail::IndexAnnotation<A>) {
      constexpr A index_annotation = std::meta::extract<A>(a);
      constexpr std::string_view name = std::define_static_string(
          index_annotation.index_name(schema::table_name_of<T>()));
      constexpr std::string_view body = std::define_static_string(
          index_annotation.index_sql_body(schema::table_name_of<T>()));
      ConstraintMetadata constraint_meta;
      constraint_meta.name = std::string(name);
      constraint_meta.type = "INDEX";
      constraint_meta.sql_definition = std::string(body);
      metadata.constraints[constraint_meta.name] = std::move(constraint_meta);
    }
  }
}

// clang-format on

}  // namespace detail

/// @brief Extract table metadata using reflection
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

    refl::for_each_field(table_instance, [&](const auto& field) {
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
          // Inline constraint modifiers (unique/check/fk + actions) are surfaced as
          // table-level constraints so their changes diff as ADD/DROP CONSTRAINT,
          // not a data-destroying DROP COLUMN + ADD COLUMN
          constexpr bool hoisted = [] {
            if constexpr (requires { field_type::has_hoisted_constraints; }) {
              return field_type::has_hoisted_constraints;
            } else {
              return false;
            }
          }();
          if constexpr (hoisted) {
            col_meta.sql_definition = field.sql_definition_sans_constraints();
            for (std::string& def : field_type::hoisted_constraint_definitions()) {
              detail::add_constraint_metadata(metadata, std::move(def));
            }
          } else {
            col_meta.sql_definition = field.sql_definition();
          }
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
      }
    });

    // Check if any errors occurred during field processing
    if (error.has_value()) {
      return std::unexpected(*error);
    }

    // Annotated tables carry cross-column constraints as struct annotations, not
    // members - collect those too
    if constexpr (requires { typename Table::annotated_type; }) {
      detail::add_annotation_constraint_metadata<typename Table::annotated_type>(metadata);
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
    return "ALTER TABLE " + schema::quote_identifier(table_name_) + " ADD COLUMN " +
           column_.sql_definition + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (column_.name.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column name cannot be empty", table_name_));
    }
    return "ALTER TABLE " + schema::quote_identifier(table_name_) + " DROP COLUMN " +
           schema::quote_identifier(column_.name) + ";";
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
    return "ALTER TABLE " + schema::quote_identifier(table_name_) + " DROP COLUMN " +
           schema::quote_identifier(column_.name) + ";";
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (column_.sql_definition.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column SQL definition cannot be empty",
                                                  table_name_ + "." + column_.name));
    }
    return "ALTER TABLE " + schema::quote_identifier(table_name_) + " ADD COLUMN " +
           column_.sql_definition + ";";
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