#pragma once

#include "../reflect.hpp"
#include "../schema/annotated_table.hpp"
#include "../schema/column.hpp"
#include "column_expression.hpp"
#include "meta.hpp"

#include <array>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

/// @brief Reflection helpers for struct-based writes: insert(t).values_from(obj),
/// insert(t).values_from(obj).upsert(), update(t).set_from(patch).
namespace relx::query::detail {

// clang-format off

/// @brief Modifier flags of a schema::column specialization
template <typename C>
struct column_mods;

template <typename M>
struct is_auto_generated_modifier : std::false_type {};
template <>
struct is_auto_generated_modifier<schema::autoincrement> : std::true_type {};
template <auto Start, auto Increment, auto MinValue, auto MaxValue, bool Cycle>
struct is_auto_generated_modifier<schema::identity<Start, Increment, MinValue, MaxValue, Cycle>>
    : std::true_type {};

template <typename TableT, schema::fixed_string Name, typename T, typename... Modifiers>
struct column_mods<schema::column<TableT, Name, T, Modifiers...>> {
  static constexpr bool is_pk = (std::is_same_v<Modifiers, schema::primary_key> || ...);
  /// GENERATED ALWAYS AS IDENTITY columns: the database assigns the value, and
  /// PostgreSQL rejects explicit inserts into them
  static constexpr bool is_auto_generated =
      (is_auto_generated_modifier<Modifiers>::value || ...);
};

/// @brief Column members of Table that accept explicit INSERT values (auto-generated
/// identity columns are skipped)
template <typename Table>
consteval auto insertable_columns() {
  std::vector<std::meta::info> cols;
  template for (constexpr std::meta::info m : select_all_columns<Table>()) {
    using C = typename [:std::meta::type_of(m):];
    if constexpr (!column_mods<C>::is_auto_generated) {
      cols.push_back(m);
    }
  }
  return std::define_static_array(cols);
}

template <typename Table>
consteval std::size_t pk_column_count() {
  std::size_t n = 0;
  template for (constexpr std::meta::info m : select_all_columns<Table>()) {
    using C = typename [:std::meta::type_of(m):];
    if constexpr (column_mods<C>::is_pk) {
      ++n;
    }
  }
  if constexpr (requires { typename Table::annotated_type; }) {
    // dealias: reflecting the member alias directly would yield an empty annotation list
    for (std::meta::info a : std::meta::annotations_of(
             std::meta::dealias(^^typename Table::annotated_type))) {
      if (std::meta::remove_cv(std::meta::type_of(a)) == ^^schema::composite_pk) {
        n += std::meta::extract<schema::composite_pk>(a).count_;
      }
    }
  }
  return n;
}

/// @brief SQL names of Table's primary-key columns: pk-modifier columns plus, for
/// annotated tables, the columns named by a composite_pk annotation
template <typename Table>
consteval auto pk_column_names() {
  std::array<std::string_view, pk_column_count<Table>()> names{};
  std::size_t i = 0;
  template for (constexpr std::meta::info m : select_all_columns<Table>()) {
    using C = typename [:std::meta::type_of(m):];
    if constexpr (column_mods<C>::is_pk) {
      names[i++] = std::string_view(C::name);
    }
  }
  if constexpr (requires { typename Table::annotated_type; }) {
    for (std::meta::info a : std::meta::annotations_of(
             std::meta::dealias(^^typename Table::annotated_type))) {
      if (std::meta::remove_cv(std::meta::type_of(a)) == ^^schema::composite_pk) {
        const auto v = std::meta::extract<schema::composite_pk>(a);
        for (std::size_t j = 0; j < v.count_; ++j) {
          names[i++] = std::define_static_string(v.at(j));
        }
      }
    }
  }
  return names;
}

/// @brief The member info of Obj's field matching column member M's SQL name (null
/// reflection when absent — values_from_diagnostics reports that as a compile error
/// before any splice happens)
template <typename Obj, std::meta::info M>
consteval std::meta::info obj_field_for_column() {
  using C = typename [:std::meta::type_of(M):];
  return refl::field_named<Obj>(std::string_view(C::name));
}

/// @brief The SQL column names of a tuple of ColumnRef<...> (an insert column list)
template <typename Columns>
struct insert_column_names;

template <typename... Cols>
struct insert_column_names<std::tuple<ColumnRef<Cols>...>> {
  static consteval auto names() {
    return std::array<std::string_view, sizeof...(Cols)>{std::string_view(Cols::name)...};
  }
};

/// @brief Whether every primary-key column of Table appears in the insert column list
template <typename Table, typename Columns>
consteval bool pk_covered_by_insert_columns() {
  constexpr auto inserted = insert_column_names<Columns>::names();
  for (std::string_view pk : pk_column_names<Table>()) {
    bool found = false;
    for (std::string_view name : inserted) {
      found = found || name == pk;
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

/// @brief Problems with mapping Obj's fields onto Table's insertable columns, empty
/// when valid. Collected as text so the static_assert message can name the offenders.
template <typename Table, typename Obj>
consteval std::string values_from_diagnostics() {
  std::string diag;
  template for (constexpr std::meta::info m : insertable_columns<Table>()) {
    using C = typename [:std::meta::type_of(m):];
    constexpr std::string_view col_name{C::name};
    if constexpr (!refl::has_field_named<Obj>(col_name)) {
      diag += "no field named '" + std::string(col_name) + "'; ";
    } else {
      using F = typename [:std::meta::type_of(refl::field_named<Obj>(col_name)):];
      if constexpr (!std::is_convertible_v<F, typename C::value_type>) {
        diag += "field '" + std::string(col_name) + "' is not convertible to the column type; ";
      }
    }
  }
  return diag;
}

template <typename T>
struct is_optional_field : std::false_type {};
template <typename T>
struct is_optional_field<std::optional<T>> : std::true_type {};

/// @brief The SQL column names of Table, for patch-field validation
template <typename Table>
consteval bool has_column_named(std::string_view name) {
  bool found = false;
  template for (constexpr std::meta::info m : select_all_columns<Table>()) {
    using C = typename [:std::meta::type_of(m):];
    found = found || std::string_view(C::name) == name;
  }
  return found;
}

template <typename Table>
consteval std::meta::info column_member_named(std::string_view name) {
  template for (constexpr std::meta::info m : select_all_columns<Table>()) {
    using C = typename [:std::meta::type_of(m):];
    if (std::string_view(C::name) == name) {
      return m;
    }
  }
  return {};
}

/// @brief Problems with a patch struct for set_from, empty when valid: every field must
/// be a std::optional whose value type is compatible with the same-named column
template <typename Table, typename Patch>
consteval std::string set_from_diagnostics() {
  std::string diag;
  template for (constexpr std::meta::info f :
                refl::member_array<Patch>()) {
    constexpr std::string_view name =
        std::define_static_string(std::meta::identifier_of(f));
    using F = typename [:std::meta::type_of(f):];
    if constexpr (!is_optional_field<F>::value) {
      diag += "field '" + std::string(name) + "' must be a std::optional; ";
    } else if constexpr (!has_column_named<Table>(name)) {
      diag += "'" + std::string(name) + "' is not a column of this table; ";
    } else {
      using C = typename [:std::meta::type_of(column_member_named<Table>(name)):];
      if constexpr (!std::is_convertible_v<F, typename C::value_type> &&
                    !std::is_convertible_v<typename F::value_type, typename C::value_type>) {
        diag += "field '" + std::string(name) + "' is not convertible to the column type; ";
      }
    }
  }
  return diag;
}

// clang-format on

}  // namespace relx::query::detail
