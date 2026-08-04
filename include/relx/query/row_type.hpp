#pragma once

#include "../schema/fixed_string.hpp"
#include "column_expression.hpp"
#include "core.hpp"

#include <meta>
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

  explicit TypedAlias(Expr expr) : expr_(std::move(expr)) {}

  std::string to_sql() const override {
    return expr_.to_sql() + " AS " + std::string(std::string_view(alias_name));
  }
  std::vector<std::string> bind_params() const override { return expr_.bind_params(); }
  std::string column_name() const override { return std::string(std::string_view(alias_name)); }
  std::string table_name() const override { return ""; }

private:
  Expr expr_;
};

/// @brief as<"alias">(expr) - result type deduced from the expression
template <schema::fixed_string Name, SqlExpr Expr>
  requires requires { typename Expr::value_type; }
auto as(Expr expr) {
  return TypedAlias<Name, typename Expr::value_type, Expr>(std::move(expr));
}

/// @brief as<"alias", T>(expr) - explicit result type, for aggregates and other
/// expressions that do not declare a value_type
template <schema::fixed_string Name, typename T, SqlExpr Expr>
auto as(Expr expr) {
  return TypedAlias<Name, T, Expr>(std::move(expr));
}

/// @brief as<"alias">(column) - alias a schema column directly
template <schema::fixed_string Name, ColumnType Column>
auto as(const Column& column) {
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

template <typename Tuple>
struct row_key;

template <typename... Es>
struct row_key<std::tuple<Es...>> {
  using type = std::tuple<decltype(row_member_key<Es>())...>;
};

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

/// @brief The aggregate type a query's rows map onto, synthesized from its select list
template <typename Query>
using row_type_for = typename detail::row_holder<
    typename detail::row_key<typename std::remove_cvref_t<Query>::columns_type>::type>::type;

/// @brief Concept for queries whose row type can be synthesized (i.e. select queries)
template <typename Query>
concept RowSynthesizable = SqlExpr<Query> && requires { typename Query::columns_type; };

}  // namespace relx::query

namespace relx {
using query::row_type_for;
}  // namespace relx
