#pragma once

#include "core.hpp"
#include "diff.hpp"

#include <string>

namespace relx::migrations {

namespace detail {

/// @brief The ADD clause with the tracked name embedded: an unnamed definition would
/// get a server-generated name the differ can't know, and the paired DROP CONSTRAINT
/// must target a name that actually exists
inline std::string add_constraint_clause(const ConstraintMetadata& constraint) {
  if (constraint.sql_definition.starts_with("CONSTRAINT ")) {
    return constraint.sql_definition;  // explicitly named via .named()
  }
  return "CONSTRAINT " + schema::quote_identifier(constraint.name) + " " +
         constraint.sql_definition;
}

}  // namespace detail

/// @brief ADD CONSTRAINT migration operation
class AddConstraintOperation : public MigrationOperation {
private:
  std::string table_name_;
  ConstraintMetadata constraint_;

public:
  AddConstraintOperation(std::string table_name, const ConstraintMetadata& constraint)
      : table_name_(std::move(table_name)), constraint_(constraint) {}

  MigrationResult<std::string> to_sql() const override {
    if (constraint_.sql_definition.empty()) {
      return std::unexpected(MigrationError::make(
          MigrationErrorType::VALIDATION_FAILED, "Constraint SQL definition cannot be empty",
          table_name_ + " constraint: " + constraint_.name));
    }

    if (constraint_.type == "INDEX") {
      // Indexes use CREATE INDEX syntax, not ALTER TABLE
      return "CREATE " + constraint_.sql_definition + ";";
    } else {
      return "ALTER TABLE " + schema::quote_identifier(table_name_) + " ADD " +
             detail::add_constraint_clause(constraint_) + ";";
    }
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (constraint_.name.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Constraint name cannot be empty for rollback",
                                                  table_name_));
    }

    if (constraint_.type == "INDEX") {
      return "DROP INDEX IF EXISTS " + schema::quote_identifier(constraint_.name) + ";";
    } else {
      return "ALTER TABLE " + schema::quote_identifier(table_name_) + " DROP CONSTRAINT " +
             schema::quote_identifier(constraint_.name) + ";";
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

  MigrationResult<std::string> to_sql() const override {
    if (constraint_.name.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Constraint name cannot be empty", table_name_));
    }

    if (constraint_.type == "INDEX") {
      return "DROP INDEX IF EXISTS " + schema::quote_identifier(constraint_.name) + ";";
    } else {
      return "ALTER TABLE " + schema::quote_identifier(table_name_) + " DROP CONSTRAINT " +
             schema::quote_identifier(constraint_.name) + ";";
    }
  }

  MigrationResult<std::string> rollback_sql() const override {
    if (constraint_.sql_definition.empty()) {
      return std::unexpected(
          MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                               "Constraint SQL definition cannot be empty for rollback",
                               table_name_ + " constraint: " + constraint_.name));
    }

    if (constraint_.type == "INDEX") {
      return "CREATE " + constraint_.sql_definition + ";";
    } else {
      return "ALTER TABLE " + schema::quote_identifier(table_name_) + " ADD " +
             detail::add_constraint_clause(constraint_) + ";";
    }
  }

  OperationType type() const override {
    return constraint_.type == "INDEX" ? OperationType::DROP_INDEX : OperationType::DROP_CONSTRAINT;
  }
};

/// @brief MODIFY COLUMN migration operation: in-place ALTER COLUMN statements
/// (TYPE ... USING cast, SET/DROP NOT NULL, SET/DROP DEFAULT) that preserve the
/// column's data, instead of a data-destroying DROP + ADD.
class ModifyColumnOperation : public MigrationOperation {
private:
  std::string table_name_;
  ColumnMetadata old_column_;
  ColumnMetadata new_column_;

  /// @brief The DEFAULT expression in a generated column definition, or empty.
  /// The expression runs to the next generated constraint keyword or the end.
  static std::string default_expression(const std::string& definition) {
    constexpr std::string_view marker = " DEFAULT ";
    const std::size_t pos = definition.find(marker);
    if (pos == std::string::npos) {
      return {};
    }
    const std::size_t start = pos + marker.size();
    std::size_t end = definition.size();
    for (const std::string_view following :
         {std::string_view(" NOT NULL"), std::string_view(" PRIMARY KEY"),
          std::string_view(" UNIQUE"), std::string_view(" CHECK"),
          std::string_view(" REFERENCES")}) {
      const std::size_t at = definition.find(following, start);
      if (at != std::string::npos && at < end) {
        end = at;
      }
    }
    return definition.substr(start, end - start);
  }

  /// @brief The definition with the parts ALTER COLUMN can change (type, NOT NULL,
  /// DEFAULT) removed - what remains must match for an in-place modify to be complete
  static std::string residual_definition(const ColumnMetadata& column) {
    std::string def = detail::normalize_whitespace(column.sql_definition);
    const auto erase_first = [&def](const std::string& part) {
      const std::size_t at = def.find(part);
      if (at != std::string::npos) {
        def.erase(at, part.size());
      }
    };
    erase_first(schema::quote_identifier(column.name) + " " + column.sql_type);
    erase_first(" NOT NULL");
    const std::string default_expr = default_expression(column.sql_definition);
    if (!default_expr.empty()) {
      erase_first(" DEFAULT " + detail::normalize_whitespace(default_expr));
    }
    return def;
  }

  MigrationResult<std::string> alter_sql(const ColumnMetadata& from,
                                         const ColumnMetadata& to) const {
    if (to.name.empty() || to.sql_type.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column name and SQL type cannot be empty",
                                                  table_name_ + "." + to.name));
    }

    const std::string column = schema::quote_identifier(to.name);
    const std::string prefix = "ALTER TABLE " + schema::quote_identifier(table_name_) +
                               " ALTER COLUMN " + column;

    std::string sql;
    if (from.sql_type != to.sql_type) {
      sql += prefix + " TYPE " + to.sql_type + " USING " + column + "::" + to.sql_type + ";";
    }
    if (from.nullable != to.nullable) {
      if (!sql.empty()) {
        sql += "\n";
      }
      sql += prefix + (to.nullable ? " DROP NOT NULL;" : " SET NOT NULL;");
    }
    const std::string old_default = default_expression(from.sql_definition);
    const std::string new_default = default_expression(to.sql_definition);
    if (old_default != new_default) {
      if (!sql.empty()) {
        sql += "\n";
      }
      sql += prefix +
             (new_default.empty() ? " DROP DEFAULT;" : " SET DEFAULT " + new_default + ";");
    }
    if (sql.empty()) {
      return std::unexpected(MigrationError::make(
          MigrationErrorType::UNSUPPORTED_OPERATION,
          "Column change cannot be expressed as ALTER COLUMN", table_name_ + "." + to.name));
    }
    return sql;
  }

public:
  ModifyColumnOperation(std::string table_name, const ColumnMetadata& old_column,
                        const ColumnMetadata& new_column)
      : table_name_(std::move(table_name)), old_column_(old_column), new_column_(new_column) {}

  /// @brief Whether the difference between two column versions is fully covered by
  /// type/nullability/default changes; anything else needs a different strategy
  static bool can_express(const ColumnMetadata& old_column, const ColumnMetadata& new_column) {
    return residual_definition(old_column) == residual_definition(new_column);
  }

  MigrationResult<std::string> to_sql() const override {
    return alter_sql(old_column_, new_column_);
  }

  MigrationResult<std::string> rollback_sql() const override {
    return alter_sql(new_column_, old_column_);
  }

  OperationType type() const override { return OperationType::MODIFY_COLUMN; }
};

}  // namespace relx::migrations