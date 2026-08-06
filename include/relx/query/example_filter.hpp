#pragma once

#include "../refl_types.hpp"
#include "../schema/identifier.hpp"
#include "core.hpp"
#include "value.hpp"
#include "write_meta.hpp"

#include <string>
#include <string_view>
#include <vector>

/// @brief Filter-by-example: where_equals(table, partialObj) builds a condition
/// ANDing `table.col = value` for every engaged field of a partial-shaped struct.
namespace relx::query {

/// @brief Condition built at runtime from the engaged fields of an example struct.
/// With no engaged fields it renders TRUE - an example with no filters matches every
/// row, which is the natural filter-by-example semantic.
class ExampleCondition : public SqlExpression {
public:
  struct Filter {
    std::string qualified_column;  ///< table.column, quoted as needed
    bind_param param;
  };

  std::vector<Filter> filters;

  std::string to_sql() const override {
    if (filters.empty()) {
      return "TRUE";
    }
    std::string out = "(";
    bool first = true;
    for (const Filter& filter : filters) {
      if (!first) {
        out += " AND ";
      }
      first = false;
      out += filter.qualified_column + " = ?";
    }
    out += ")";
    return out;
  }

  std::vector<bind_param> bind_params() const override {
    std::vector<bind_param> params;
    params.reserve(filters.size());
    for (const Filter& filter : filters) {
      params.push_back(filter.param);
    }
    return params;
  }
};

/// @brief Build an equality filter from the engaged fields of an example struct (a
/// struct of std::optional fields named after columns of the table, e.g.
/// relx::refl::partial<Users>). Disengaged fields do not filter; engaged fields
/// become `table.column = ?`. NULL matching is not expressible by example - use an
/// explicit is_null() condition for that.
///
/// ```cpp
/// relx::refl::partial<Users> example{};
/// example.active = true;
/// auto rows = conn.fetch_all(select_all(users).where(relx::where_equals(users, example)));
/// ```
template <TableType Table, typename Example>
  requires(!SqlExpr<Example> && !ColumnType<Example>)
ExampleCondition where_equals(const Table& /*table*/, const Example& example) {
  static_assert(refl::field_count<Example>() > 0,
                "where_equals() requires an example struct with at least one field");
  static_assert(detail::set_from_diagnostics<Table, Example>().empty(),
                std::string("where_equals(): example does not match the table's columns: ") +
                    detail::set_from_diagnostics<Table, Example>());

  ExampleCondition condition;
  refl::for_each_named_field(example, [&](const auto& field, std::string_view name) {
    if (field.has_value()) {
      using FieldValue = std::remove_cvref_t<decltype(*field)>;
      auto params = Value<FieldValue>(*field).bind_params();
      condition.filters.push_back({schema::quote_identifier(std::string_view(Table::table_name)) +
                                       "." + schema::quote_identifier(name),
                                   std::move(params.front())});
    }
  });
  return condition;
}

}  // namespace relx::query
