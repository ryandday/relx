#pragma once

#include "core.hpp"
#include "diff.hpp"

#include <string>

namespace relx::migrations {

namespace detail {

/// @brief The ADD clause with the tracked name embedded: an unnamed definition would
/// get a server-generated name the differ can't know, and the paired DROP CONSTRAINT
/// must target a name that actually exists. The name is always quoted through
/// quote_identifier - the same treatment DROP CONSTRAINT applies - so explicitly
/// .named() mixed-case constraints add and drop under the same identifier.
inline std::string add_constraint_clause(const ConstraintMetadata& constraint) {
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
  /// Caller-provided USING expressions for the type cast (forward / backward);
  /// empty means the plain `column::type` cast
  std::string using_forward_;
  std::string using_backward_;

  /// @brief Find `what` in `definition` starting at `from`, skipping matches inside
  /// single-quoted SQL string literals ('' escapes a quote)
  static std::size_t find_outside_literals(const std::string& definition, std::string_view what,
                                           std::size_t from = 0) {
    bool in_literal = false;
    for (std::size_t i = from; i + what.size() <= definition.size(); ++i) {
      const char c = definition[i];
      if (c == '\'') {
        in_literal = !in_literal;
        continue;
      }
      if (!in_literal && definition.compare(i, what.size(), what) == 0) {
        return i;
      }
    }
    return std::string::npos;
  }

  /// @brief The DEFAULT expression in a generated column definition, or empty.
  /// The expression runs to the next generated constraint keyword or the end;
  /// keywords inside string literals (DEFAULT 'no CHECK needed') do not count.
  static std::string default_expression(const std::string& definition) {
    constexpr std::string_view marker = " DEFAULT ";
    const std::size_t pos = find_outside_literals(definition, marker);
    if (pos == std::string::npos) {
      return {};
    }
    const std::size_t start = pos + marker.size();
    std::size_t end = definition.size();
    for (const std::string_view following :
         {std::string_view(" NOT NULL"), std::string_view(" PRIMARY KEY"),
          std::string_view(" UNIQUE"), std::string_view(" CHECK"),
          std::string_view(" REFERENCES")}) {
      const std::size_t at = find_outside_literals(definition, following, start);
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

  MigrationResult<std::string> alter_sql(const ColumnMetadata& from, const ColumnMetadata& to,
                                         const std::string& using_expr) const {
    if (to.name.empty() || to.sql_type.empty()) {
      return std::unexpected(MigrationError::make(MigrationErrorType::VALIDATION_FAILED,
                                                  "Column name and SQL type cannot be empty",
                                                  table_name_ + "." + to.name));
    }

    const std::string column = schema::quote_identifier(to.name);
    const std::string prefix = "ALTER TABLE " + schema::quote_identifier(table_name_) +
                               " ALTER COLUMN " + column;

    const bool type_changed = from.sql_type != to.sql_type;
    const std::string old_default = default_expression(from.sql_definition);
    const std::string new_default = default_expression(to.sql_definition);

    std::string sql;
    const auto append = [&sql](std::string clause) {
      if (!sql.empty()) {
        sql += "\n";
      }
      sql += std::move(clause);
    };

    // Order matters: an existing DEFAULT may not auto-cast to the new type, so it is
    // dropped BEFORE the type change and re-established afterwards
    if (type_changed && !old_default.empty()) {
      append(prefix + " DROP DEFAULT;");
    }
    if (type_changed) {
      const std::string cast = using_expr.empty() ? column + "::" + to.sql_type : using_expr;
      append(prefix + " TYPE " + to.sql_type + " USING " + cast + ";");
    }
    if (from.nullable != to.nullable) {
      append(prefix + (to.nullable ? " DROP NOT NULL;" : " SET NOT NULL;"));
    }
    // Re-set the default when it changed, or when the type change dropped it above
    if (!new_default.empty() && (old_default != new_default || type_changed)) {
      append(prefix + " SET DEFAULT " + new_default + ";");
    } else if (new_default.empty() && !old_default.empty() && !type_changed) {
      append(prefix + " DROP DEFAULT;");
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

  /// @brief With caller-provided USING expressions for the type cast: `using_forward`
  /// converts old values to the new type, `using_backward` the reverse. Truncating or
  /// lossy casts should be written explicitly here rather than trusting `col::type`.
  ModifyColumnOperation(std::string table_name, const ColumnMetadata& old_column,
                        const ColumnMetadata& new_column, std::string using_forward,
                        std::string using_backward)
      : table_name_(std::move(table_name)), old_column_(old_column), new_column_(new_column),
        using_forward_(std::move(using_forward)), using_backward_(std::move(using_backward)) {}

  /// @brief Whether the difference between two column versions is fully covered by
  /// type/nullability/default changes; anything else needs a different strategy
  static bool can_express(const ColumnMetadata& old_column, const ColumnMetadata& new_column) {
    return residual_definition(old_column) == residual_definition(new_column);
  }

  MigrationResult<std::string> to_sql() const override {
    return alter_sql(old_column_, new_column_, using_forward_);
  }

  MigrationResult<std::string> rollback_sql() const override {
    return alter_sql(new_column_, old_column_, using_backward_);
  }

  OperationType type() const override { return OperationType::MODIFY_COLUMN; }
};

}  // namespace relx::migrations