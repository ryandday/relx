#pragma once

#include "../migrations/diff.hpp"
#include "../query/write_meta.hpp"
#include "../schema/annotated_table.hpp"
#include "connection.hpp"

#include <algorithm>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// @brief Boot-time schema-drift verification: relx::verify_schema<Users, Posts>(conn)
/// diffs the reflected table metadata against the live database's information_schema
/// and returns a structured report of every difference. No migration is emitted -
/// this is a read-only check for "the binary and the database disagree".
namespace relx::connection {

/// @brief One column-level difference between the reflected schema and the database
struct ColumnDrift {
  enum class Kind {
    MissingColumn,        ///< declared in C++, absent in the database
    ExtraColumn,          ///< present in the database, not declared in C++
    TypeMismatch,         ///< SQL types differ
    NullabilityMismatch,  ///< NOT NULL vs nullable differ
  };

  Kind kind;
  std::string column;
  std::string expected;  ///< the C++-side view (empty for ExtraColumn)
  std::string actual;    ///< the database-side view (empty for MissingColumn)
};

/// @brief Every difference found for one table
struct TableDrift {
  std::string table;
  bool missing_table = false;
  std::vector<ColumnDrift> columns;
  /// Primary-key column sets, in declaration order, when they differ
  bool pk_mismatch = false;
  std::vector<std::string> pk_expected;
  std::vector<std::string> pk_actual;

  bool clean() const { return !missing_table && columns.empty() && !pk_mismatch; }
};

/// @brief verify_schema error: either the introspection queries failed, or drift was
/// found (drift lists only tables with differences)
struct SchemaDriftError {
  std::optional<ConnectionError> connection_error;
  std::vector<TableDrift> drift;

  /// @brief Human-readable one-string summary of everything that differs
  std::string message() const {
    if (connection_error) {
      return "schema verification could not run: " + connection_error->message;
    }
    std::string out;
    for (const TableDrift& table : drift) {
      if (!out.empty()) {
        out += "; ";
      }
      if (table.missing_table) {
        out += "table '" + table.table + "' does not exist";
        continue;
      }
      out += "table '" + table.table + "':";
      for (const ColumnDrift& column : table.columns) {
        switch (column.kind) {
        case ColumnDrift::Kind::MissingColumn:
          out += " column '" + column.column + "' missing (expected " + column.expected + ")";
          break;
        case ColumnDrift::Kind::ExtraColumn:
          out += " unexpected column '" + column.column + "' (" + column.actual + ")";
          break;
        case ColumnDrift::Kind::TypeMismatch:
          out += " column '" + column.column + "' type is " + column.actual + ", expected " +
                 column.expected;
          break;
        case ColumnDrift::Kind::NullabilityMismatch:
          out += " column '" + column.column + "' is " + column.actual + ", expected " +
                 column.expected;
          break;
        }
      }
      if (table.pk_mismatch) {
        const auto joined = [](const std::vector<std::string>& names) {
          std::string list;
          for (const std::string& name : names) {
            list += list.empty() ? "" : ", ";
            list += name;
          }
          return list.empty() ? std::string("<none>") : list;
        };
        out += " primary key is (" + joined(table.pk_actual) + "), expected (" +
               joined(table.pk_expected) + ")";
      }
    }
    return out;
  }
};

template <typename T>
using VerifySchemaResult = std::expected<T, SchemaDriftError>;

namespace detail {

/// @brief Normalize a relx SQL type name to information_schema's data_type spelling
/// (lowercase, length arguments stripped, known aliases expanded)
inline std::string normalize_sql_type(std::string_view type) {
  std::string upper;
  for (const char c : type) {
    upper += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
  }
  // Strip a parenthesized length/precision argument: VARCHAR(255) -> VARCHAR
  if (const std::size_t open = upper.find('('); open != std::string::npos) {
    upper.resize(open);
    while (!upper.empty() && upper.back() == ' ') {
      upper.pop_back();
    }
  }

  if (upper == "TIMESTAMPTZ") {
    return "timestamp with time zone";
  }
  if (upper == "TIMESTAMP") {
    return "timestamp without time zone";
  }
  if (upper == "VARCHAR" || upper == "CHARACTER VARYING") {
    return "character varying";
  }
  if (upper == "INT" || upper == "INT4" || upper == "SERIAL") {
    return "integer";
  }
  if (upper == "INT8" || upper == "BIGSERIAL") {
    return "bigint";
  }
  if (upper == "INT2") {
    return "smallint";
  }
  if (upper == "FLOAT8") {
    return "double precision";
  }
  if (upper == "FLOAT4") {
    return "real";
  }
  if (upper == "DECIMAL") {
    return "numeric";
  }

  std::string lower;
  for (const char c : upper) {
    lower += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }
  return lower;
}

/// @brief The database's view of one column
struct DbColumn {
  std::string name;
  std::string data_type;  ///< normalized: udt_name for USER-DEFINED, else data_type
  bool nullable = false;
};

/// @brief Introspect one table's columns; empty vector means the table does not exist
inline VerifySchemaResult<std::vector<DbColumn>> introspect_columns(Connection& conn,
                                                                    const std::string& table) {
  auto result = conn.execute_raw("SELECT column_name, data_type, udt_name, is_nullable "
                                 "FROM information_schema.columns "
                                 "WHERE table_schema = current_schema() AND table_name = ? "
                                 "ORDER BY ordinal_position",
                                 {bind_param(table)});
  if (!result) {
    return std::unexpected(SchemaDriftError{.connection_error = result.error()});
  }

  std::vector<DbColumn> columns;
  for (std::size_t i = 0; i < result->size(); ++i) {
    const auto& row = result->at(i);
    DbColumn column;
    auto name = row.get<std::string>(0);
    auto data_type = row.get<std::string>(1);
    auto udt_name = row.get<std::string>(2);
    auto is_nullable = row.get<std::string>(3);
    if (!name || !data_type || !udt_name || !is_nullable) {
      return std::unexpected(SchemaDriftError{
          .connection_error = ConnectionError{.message = "information_schema returned "
                                                         "unreadable rows"}});
    }
    column.name = *name;
    column.data_type = (*data_type == "USER-DEFINED") ? *udt_name : normalize_sql_type(*data_type);
    column.nullable = (*is_nullable == "YES");
    columns.push_back(std::move(column));
  }
  return columns;
}

/// @brief The database's primary-key column list for a table, in key order
inline VerifySchemaResult<std::vector<std::string>> introspect_pk(Connection& conn,
                                                                  const std::string& table) {
  auto result = conn.execute_raw("SELECT kcu.column_name "
                                 "FROM information_schema.table_constraints tc "
                                 "JOIN information_schema.key_column_usage kcu "
                                 "  ON tc.constraint_name = kcu.constraint_name "
                                 " AND tc.table_schema = kcu.table_schema "
                                 "WHERE tc.constraint_type = 'PRIMARY KEY' "
                                 "  AND tc.table_schema = current_schema() AND tc.table_name = ? "
                                 "ORDER BY kcu.ordinal_position",
                                 {bind_param(table)});
  if (!result) {
    return std::unexpected(SchemaDriftError{.connection_error = result.error()});
  }

  std::vector<std::string> pk;
  for (std::size_t i = 0; i < result->size(); ++i) {
    auto name = result->at(i).get<std::string>(0);
    if (!name) {
      return std::unexpected(SchemaDriftError{
          .connection_error = ConnectionError{.message = "information_schema returned "
                                                         "unreadable rows"}});
    }
    pk.push_back(*name);
  }
  return pk;
}

/// @brief Verify one table object against the database, appending to the report
template <typename Table>
VerifySchemaResult<void> verify_one_table(Connection& conn, std::vector<TableDrift>& report) {
  const Table table_obj{};
  auto metadata_result = migrations::extract_table_metadata(table_obj);
  if (!metadata_result) {
    return std::unexpected(SchemaDriftError{
        .connection_error = ConnectionError{.message = "failed to reflect table metadata: " +
                                                       metadata_result.error().message}});
  }
  const auto& metadata = *metadata_result;

  TableDrift drift;
  drift.table = metadata.table_name;

  auto db_columns_result = introspect_columns(conn, metadata.table_name);
  if (!db_columns_result) {
    return std::unexpected(db_columns_result.error());
  }
  const auto& db_columns = *db_columns_result;

  if (db_columns.empty()) {
    drift.missing_table = true;
    report.push_back(std::move(drift));
    return {};
  }

  // C++ -> DB: missing columns, type/nullability mismatches
  for (const auto& [name, column] : metadata.columns) {
    const auto db_it = std::find_if(db_columns.begin(), db_columns.end(),
                                    [&](const DbColumn& db) { return db.name == name; });
    const std::string expected_type = normalize_sql_type(column.sql_type);
    if (db_it == db_columns.end()) {
      drift.columns.push_back({.kind = ColumnDrift::Kind::MissingColumn,
                               .column = name,
                               .expected = expected_type,
                               .actual = ""});
      continue;
    }
    if (db_it->data_type != expected_type) {
      drift.columns.push_back({.kind = ColumnDrift::Kind::TypeMismatch,
                               .column = name,
                               .expected = expected_type,
                               .actual = db_it->data_type});
    }
    if (db_it->nullable != column.nullable) {
      drift.columns.push_back({.kind = ColumnDrift::Kind::NullabilityMismatch,
                               .column = name,
                               .expected = column.nullable ? "nullable" : "NOT NULL",
                               .actual = db_it->nullable ? "nullable" : "NOT NULL"});
    }
  }

  // DB -> C++: extra columns
  for (const DbColumn& db_column : db_columns) {
    if (!metadata.columns.contains(db_column.name)) {
      drift.columns.push_back({.kind = ColumnDrift::Kind::ExtraColumn,
                               .column = db_column.name,
                               .expected = "",
                               .actual = db_column.data_type});
    }
  }

  // Primary key column set (order-sensitive: key order is meaningful)
  constexpr auto pk_names = query::detail::pk_column_names<Table>();
  std::vector<std::string> expected_pk(pk_names.begin(), pk_names.end());
  auto db_pk_result = introspect_pk(conn, metadata.table_name);
  if (!db_pk_result) {
    return std::unexpected(db_pk_result.error());
  }
  if (expected_pk != *db_pk_result) {
    drift.pk_mismatch = true;
    drift.pk_expected = std::move(expected_pk);
    drift.pk_actual = std::move(*db_pk_result);
  }

  if (!drift.clean()) {
    report.push_back(std::move(drift));
  }
  return {};
}

/// @brief Map a type argument to its table object type: annotated structs verify via
/// their synthesized table object, table object types verify directly
template <typename T>
consteval auto table_object_id() {
  if constexpr (schema::TableConcept<T>) {
    return std::type_identity<T>{};
  } else {
    return std::type_identity<schema::table_ref<T>>{};
  }
}

template <typename T>
using table_object_t = typename decltype(table_object_id<T>())::type;

}  // namespace detail

/// @brief Verify that the database schema matches the reflected definition of every
/// given table (annotated structs or table object types). Checks table existence,
/// column presence, SQL types, nullability, and the primary-key column set.
/// Returns the structured drift report as the error; connection failures during
/// introspection are reported distinctly.
///
/// ```cpp
/// if (auto verified = relx::verify_schema<Users, Posts>(conn); !verified) {
///   log_fatal(verified.error().message());
/// }
/// ```
template <typename... Tables>
  requires(sizeof...(Tables) > 0)
VerifySchemaResult<void> verify_schema(Connection& conn) {
  std::vector<TableDrift> report;
  SchemaDriftError connection_failure;
  bool failed = false;

  auto verify = [&]<typename T>(std::type_identity<T>) {
    if (failed) {
      return;
    }
    auto result = detail::verify_one_table<detail::table_object_t<T>>(conn, report);
    if (!result) {
      connection_failure = std::move(result.error());
      failed = true;
    }
  };
  (verify(std::type_identity<Tables>{}), ...);

  if (failed) {
    return std::unexpected(std::move(connection_failure));
  }
  if (!report.empty()) {
    return std::unexpected(SchemaDriftError{.drift = std::move(report)});
  }
  return {};
}

}  // namespace relx::connection

namespace relx {
using connection::ColumnDrift;
using connection::SchemaDriftError;
using connection::TableDrift;
using connection::verify_schema;
}  // namespace relx
