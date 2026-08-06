#pragma once

#include "../schema/fixed_string.hpp"
#include "column_expression.hpp"
#include "core.hpp"
#include "meta.hpp"

#include <meta>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

/// @brief Result-row types synthesized from a query's select list.
///
/// Every selected column already knows its name and C++ type, so the aggregate a row maps
/// onto can be generated instead of hand-written:
///
/// ```cpp
/// auto q = relx::select(users.id, users.username,
///                       relx::as<"post_count", long>(relx::count(posts.id)))
///              .from(users)...;
/// auto rows = conn.fetch_all(q);      // std::expected<std::vector<Row>, ...>
/// rows->front().id;                    // int
/// rows->front().post_count;            // long
/// ```
///
/// A select-list/row mismatch is impossible by construction. Expressions without a
/// compile-time name/type must be aliased with relx::as<"name">(expr) (deduces the type
/// when the expression has one) or relx::as<"name", T>(expr) (aggregates etc.).
namespace relx::query {

/// @brief Aliased expression whose alias (and result type) are compile-time constants,
/// so it can name a member of a synthesized row type
template <schema::fixed_string Name, typename T, SqlExpr Expr>
class TypedAlias : public ColumnExpression {
public:
  using value_type = T;
  static constexpr auto alias_name = Name;

  constexpr explicit TypedAlias(Expr expr) : expr_(std::move(expr)) {}

  constexpr std::string to_sql() const override {
    return expr_.to_sql() + " AS " + std::string(std::string_view(alias_name));
  }
  constexpr std::vector<bind_param> bind_params() const override { return expr_.bind_params(); }
  std::string column_name() const override { return std::string(std::string_view(alias_name)); }
  std::string table_name() const override { return ""; }

private:
  Expr expr_;
};

/// @brief as<"alias">(expr) - result type deduced from the expression
template <schema::fixed_string Name, SqlExpr Expr>
  requires requires { typename Expr::value_type; }
constexpr auto as(Expr expr) {
  return TypedAlias<Name, typename Expr::value_type, Expr>(std::move(expr));
}

/// @brief as<"alias", T>(expr) - explicit result type, for aggregates and other
/// expressions that do not declare a value_type
template <schema::fixed_string Name, typename T, SqlExpr Expr>
constexpr auto as(Expr expr) {
  return TypedAlias<Name, T, Expr>(std::move(expr));
}

/// @brief as<"alias">(column) - alias a schema column directly
template <schema::fixed_string Name, ColumnType Column>
constexpr auto as(const Column& column) {
  return TypedAlias<Name, typename Column::value_type, ColumnRef<Column>>(column_ref(column));
}

namespace detail {

// clang-format off

/// @brief Normalized description of one synthesized-row member. Row types are keyed on
/// tuples of these instead of the full query type, keeping diagnostics short and making
/// queries with the same select shape share one row type.
template <schema::fixed_string Name, typename T>
struct named {};

template <typename E>
consteval auto row_member_key() {
  if constexpr (requires {
                  typename E::column_type;
                  typename E::value_type;
                }) {
    // ColumnRef<C>: name and type from the schema column
    return named<E::column_type::name, typename E::value_type>{};
  } else if constexpr (requires { typename E::value_type; } &&
                       requires { std::string_view(E::alias_name); }) {
    // TypedAlias<Name, T, Expr>
    return named<E::alias_name, typename E::value_type>{};
  } else {
    static_assert(false,
                  "This select column cannot appear in a synthesized row: alias it with "
                  "relx::as<\"name\">(expr) (add an explicit type for aggregates: "
                  "relx::as<\"name\", long>(count(...)))");
  }
}

/// @brief Whether a select-list element stands for a whole table (see TableColumns
/// in select.hpp; detected structurally to avoid a header cycle)
template <typename E>
concept TableSelectElement = requires {
  typename E::table_type;
  requires E::is_table_columns;
};

/// @brief The table name as a fixed_string, for use as a synthesized member name
template <typename Table>
consteval auto table_member_name() {
  constexpr std::string_view sv = Table::table_name;
  schema::fixed_string<sv.size() + 1> fs{};
  for (std::size_t i = 0; i < sv.size(); ++i) {
    fs.value[i] = sv[i];
  }
  return fs;
}

/// @brief Synthesized value struct for a classic table: one member per column, holding
/// the column's value_type (the classic table struct itself holds column objects, not
/// values, so it cannot be the row member type)
template <typename Table>
struct classic_row_holder {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    template for (constexpr std::meta::info m : detail::select_all_columns<Table>()) {
      using C = typename [:std::meta::type_of(m):];
      specs.push_back(std::meta::data_member_spec(std::meta::dealias(^^typename C::value_type),
                                                  {.name = std::string_view(C::name)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};

template <typename Table>
consteval std::meta::info table_value_struct_info() {
  if constexpr (requires { typename Table::annotated_type; }) {
    // Annotated table: the user's struct is the natural value type
    return std::meta::dealias(^^typename Table::annotated_type);
  } else {
    return ^^typename classic_row_holder<Table>::type;
  }
}

/// @brief The value struct a whole-table select member maps onto
template <typename Table>
using table_value_struct_t = typename [:table_value_struct_info<Table>():];

/// @brief Whether Table is the target of a LEFT JOIN in the query's join list (its
/// nested row member becomes std::optional - a non-matching left join yields NULLs)
template <typename Table, typename Joins>
inline constexpr bool left_joined_v = false;
template <typename Table, typename... Specs>
inline constexpr bool left_joined_v<Table, std::tuple<Specs...>> =
    ((std::is_same_v<Table, typename Specs::table_type> && Specs::type == JoinType::Left) || ...);

/// @brief Whether the join list contains a RIGHT or FULL join (unsupported for nested
/// rows: they can NULL out the FROM side, which nested synthesis does not model)
template <typename Joins>
inline constexpr bool has_right_or_full_join_v = false;
template <typename... Specs>
inline constexpr bool has_right_or_full_join_v<std::tuple<Specs...>> =
    ((Specs::type == JoinType::Right || Specs::type == JoinType::Full) || ...);

template <typename E, typename Joins>
consteval auto row_member_key_joined() {
  if constexpr (TableSelectElement<E>) {
    using Table = typename E::table_type;
    using V = table_value_struct_t<Table>;
    if constexpr (left_joined_v<Table, Joins>) {
      return named<table_member_name<Table>(), std::optional<V>>{};
    } else {
      return named<table_member_name<Table>(), V>{};
    }
  } else {
    return row_member_key<E>();
  }
}

template <typename Tuple, typename Joins>
struct row_key;

template <typename... Es, typename Joins>
struct row_key<std::tuple<Es...>, Joins> {
  static_assert(!((TableSelectElement<Es> || ...) && has_right_or_full_join_v<Joins>),
                "whole-table selects cannot synthesize rows for RIGHT or FULL joins: "
                "those can NULL out the FROM side too. Select explicit columns with "
                "optional-typed aliases instead");
  using type = std::tuple<decltype(row_member_key_joined<Es, Joins>())...>;
};

template <typename Query>
consteval auto query_joins_id() {
  if constexpr (requires { typename std::remove_cvref_t<Query>::joins_type; }) {
    return std::type_identity<typename std::remove_cvref_t<Query>::joins_type>{};
  } else {
    return std::type_identity<std::tuple<>>{};
  }
}

template <typename Query>
using query_joins_t = typename decltype(query_joins_id<Query>())::type;

template <typename KeyTuple>
struct row_holder;

template <schema::fixed_string... Names, typename... Ts>
struct row_holder<std::tuple<named<Names, Ts>...>> {
  struct type;
  consteval {
    std::meta::define_aggregate(
        ^^type, {std::meta::data_member_spec(std::meta::dealias(^^Ts),
                                             {.name = std::string_view(Names)})...});
  }
};

// clang-format on

}  // namespace detail

/// @brief The aggregate type a query's rows map onto, synthesized from its result
/// columns: the select list for SELECT queries (whole-table selects become nested
/// members, std::optional when left-joined), the RETURNING list for DML queries
template <typename Query>
using row_type_for = typename detail::row_holder<
    typename detail::row_key<result_columns_t<Query>, detail::query_joins_t<Query>>::type>::type;

/// @brief Concept for queries whose row type can be synthesized: select queries and
/// DML with a RETURNING clause
template <typename Query>
concept RowSynthesizable = SqlExpr<Query> && requires { typename result_columns_t<Query>; };

}  // namespace relx::query

namespace relx {
using query::row_type_for;
}  // namespace relx
