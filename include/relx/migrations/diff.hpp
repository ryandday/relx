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

  /// Native-enum columns carry their type name and enumerator list so the differ can
  /// emit ALTER TYPE ... ADD VALUE instead of silently diffing to nothing. Excluded
  /// from operator== - enum value changes are type changes, not column changes.
  std::string enum_type_name;
  std::vector<std::string> enum_values;

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
/// drop/add pairs. An explicit `CONSTRAINT name` prefix overrides the generated name
/// and is stripped from the stored definition, so ADD/DROP always build the clause
/// with consistent identifier quoting.
inline void add_constraint_metadata(TableMetadata& metadata, std::string sql_def) {
  ConstraintMetadata constraint_meta;

  // Split off an explicit "CONSTRAINT <name> " prefix (name possibly quoted)
  std::string explicit_name;
  constexpr std::string_view name_prefix = "CONSTRAINT ";
  if (sql_def.starts_with(name_prefix)) {
    std::size_t name_start = name_prefix.size();
    std::size_t name_end = std::string::npos;
    if (name_start < sql_def.size() && sql_def[name_start] == '"') {
      const std::size_t close = sql_def.find('"', name_start + 1);
      if (close != std::string::npos) {
        explicit_name = sql_def.substr(name_start + 1, close - name_start - 1);
        name_end = close + 1;
      }
    } else {
      name_end = sql_def.find(' ', name_start);
      if (name_end != std::string::npos) {
        explicit_name = sql_def.substr(name_start, name_end - name_start);
      }
    }
    if (!explicit_name.empty() && name_end != std::string::npos) {
      std::size_t body_start = name_end;
      while (body_start < sql_def.size() && sql_def[body_start] == ' ') {
        ++body_start;
      }
      sql_def = sql_def.substr(body_start);
    } else {
      explicit_name.clear();
    }
  }

  constraint_meta.sql_definition = std::move(sql_def);

  // Classification is structural (what the definition STARTS with), never substring
  // matching: a CHECK whose expression mentions "UNIQUE" is still a CHECK
  const std::string& sql = constraint_meta.sql_definition;
  const auto content_name = [&sql](const std::string& prefix) {
    const std::string slug = constrained_columns_slug(sql);
    return slug.empty() ? prefix + content_hash(sql) : prefix + slug;
  };
  if (sql.starts_with("PRIMARY KEY")) {
    constraint_meta.type = "PRIMARY_KEY";
    constraint_meta.name = metadata.table_name + "_pk";
  } else if (sql.starts_with("FOREIGN KEY") || sql.starts_with("REFERENCES")) {
    constraint_meta.type = "FOREIGN_KEY";
    constraint_meta.name = content_name(metadata.table_name + "_fk_");
  } else if (sql.starts_with("UNIQUE INDEX") || sql.starts_with("INDEX")) {
    constraint_meta.type = "INDEX";
    constraint_meta.name = content_name(metadata.table_name + "_idx_");
  } else if (sql.starts_with("UNIQUE")) {
    constraint_meta.type = "UNIQUE";
    constraint_meta.name = content_name(metadata.table_name + "_unique_");
  } else if (sql.starts_with("CHECK")) {
    constraint_meta.type = "CHECK";
    constraint_meta.name = metadata.table_name + "_check_" + content_hash(sql);
  } else {
    constraint_meta.type = "UNKNOWN";
    constraint_meta.name = metadata.table_name + "_constraint_" + content_hash(sql);
  }

  if (!explicit_name.empty()) {
    constraint_meta.name = std::move(explicit_name);
  } else if (metadata.constraints.contains(constraint_meta.name)) {
    // Two constraints of the same shape on the same columns (e.g. two FKs from one
    // column to different targets) must not silently overwrite each other
    constraint_meta.name += "_" + content_hash(sql);
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

        if constexpr (field_type::uses_native_enum) {
          using enum_type =
              typename schema::detail::unwrap_optional<typename field_type::value_type>::type;
          col_meta.enum_type_name = std::string(schema::pg_enum_type_name<enum_type>());
          col_meta.enum_values = refl::enum_values<enum_type>();
        }

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

// clang-format off

namespace detail {

/// @brief Append CREATE INDEX / DROP INDEX operations for T's index_on annotations.
/// A separate template so `template for` expands over annotations_of(^^T) with T as a
/// direct template parameter - expanding over a dependent nested alias (^^A inside an
/// if-constexpr branch) silently iterates zero times on GCC 16.
template <typename T>
void add_index_operations(Migration& migration) {
  template for (constexpr std::meta::info a :
                std::define_static_array(std::meta::annotations_of(^^T))) {
    using Ann = typename [:std::meta::remove_cv(std::meta::type_of(a)):];
    if constexpr (schema::detail::IndexAnnotation<Ann>) {
      constexpr Ann index_annotation = std::meta::extract<Ann>(a);
      constexpr std::string_view create_sql = std::define_static_string(
          index_annotation.index_sql(schema::table_name_of<T>()));
      constexpr std::string_view index_name = std::define_static_string(
          index_annotation.index_name(schema::table_name_of<T>()));
      migration.add_operation<RawSqlOperation>(
          std::string(create_sql) + ";",
          "DROP INDEX IF EXISTS " + std::string(index_name) + ";", OperationType::ADD_INDEX);
    }
  }
}

}  // namespace detail

/// @brief Generate migration to create a new table, including everything the table
/// needs that CREATE TABLE alone does not produce: native enum CREATE TYPE statements
/// (before the table) and CREATE INDEX statements (after it). Omitting these made the
/// same schema differ depending on whether it was reached via create or via
/// incremental diff, and native-enum tables failed outright on fresh databases.
/// @tparam Table The table type
/// @param table Instance of the table to create
/// @return Migration to create the table
template <schema::TableConcept Table>
MigrationResult<Migration> generate_create_table_migration(const Table& table) {
  Migration migration("create_" + std::string(Table::table_name));

  // Native enum types must exist before the table that uses them (deduplicated:
  // two columns sharing an enum need one CREATE TYPE)
  std::unordered_set<std::string> seen_enum_types;
  refl::for_each_field(table, [&](const auto& field) {
    using field_type = std::remove_cvref_t<decltype(field)>;
    if constexpr (schema::is_column<field_type>) {
      if constexpr (field_type::uses_native_enum) {
        using enum_type = typename schema::detail::unwrap_optional<
            typename field_type::value_type>::type;
        std::string type_name(schema::pg_enum_type_name<enum_type>());
        if (seen_enum_types.insert(type_name).second) {
          migration.add_operation<RawSqlOperation>(
              std::string(schema::create_enum_type_sql<enum_type>()),
              "DROP TYPE IF EXISTS " + type_name + ";", OperationType::CREATE_TABLE);
        }
      }
    }
  });

  migration.add_operation<CreateTableOperation<Table>>(table);

  // Indexes are separate statements after the table exists
  if constexpr (requires { typename Table::annotated_type; }) {
    detail::add_index_operations<typename Table::annotated_type>(migration);
  }

  return migration;
}

// clang-format on

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