#pragma once

#include "../reflect.hpp"
#include "column.hpp"
#include "fixed_string.hpp"
#include "table.hpp"

#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

/// @brief Annotation-based table definitions built on C++26 reflection (P2996 + P3394).
///
/// A table is a plain aggregate annotated with relx annotations:
///
/// ```cpp
/// struct [[=relx::table("users")]] Users {
///   [[=relx::ann::pk]] int id;
///   std::string username;
///   [[=relx::ann::unique]] std::string email;
///   [[=relx::default_value<true>{}]] bool active;
///   std::optional<std::string> bio;  // nullable
/// };
/// ```
///
/// Column names come from the member identifiers, the table name from the
/// [[=relx::table("...")]] annotation (falling back to the struct identifier), and
/// modifiers from annotations. The same struct doubles as the DTO for query results.
///
/// Next to the struct, define its table object once — reflection synthesizes one column
/// member per field, and queries read naturally:
///
/// ```cpp
/// inline constexpr auto users = relx::t<Users>;
///
/// auto q = relx::select(users.id, users.username)
///              .from(users)
///              .where(users.id == 42);
/// auto rows = conn.execute_many<Users>(q);
/// ```
///
/// relx::c<^^Users::id> remains available as a standalone column reference for contexts
/// where no table object is in scope.
namespace relx::schema {

// clang-format off

/// @brief Struct-level annotation carrying the SQL table name: [[=relx::table("users")]]
struct table {
  static constexpr std::size_t max_length = 64;

  char name_[max_length] = {};
  std::size_t len_ = 0;

  // A name longer than max_length is a compile error (out-of-bounds write in consteval)
  consteval table(const char* s) {
    while (s[len_] != '\0') {
      name_[len_] = s[len_];
      ++len_;
    }
  }

  constexpr std::string_view view() const { return std::string_view(name_, len_); }
};

/// @brief Concept for column modifier types (UNIQUE, PRIMARY KEY, DEFAULT ...)
template <typename T>
concept ColumnModifier = requires { T::to_sql(); };

namespace detail {

template <ColumnModifier T>
struct modifier_probe {};

consteval bool is_modifier_type(std::meta::info type) {
  return std::meta::can_substitute(^^modifier_probe, {type});
}

/// @brief Member identifier as a fixed_string, for use as the column Name NTTP
template <std::meta::info M>
consteval auto member_name_fs() {
  constexpr std::string_view sv = std::meta::identifier_of(M);
  fixed_string<sv.size() + 1> fs{};
  for (std::size_t i = 0; i < sv.size(); ++i) {
    fs.value[i] = sv[i];
  }
  return fs;
}

}  // namespace detail

/// @brief SQL table name of an annotated struct: the [[=relx::table("...")]] annotation,
/// or the struct identifier when no annotation is present
template <typename T>
consteval std::string_view table_name_of() {
  for (std::meta::info a : std::meta::annotations_of(^^T)) {
    if (std::meta::remove_cv(std::meta::type_of(a)) == ^^table) {
      return std::define_static_string(std::meta::extract<table>(a).view());
    }
  }
  return std::define_static_string(std::meta::identifier_of(^^T));
}

namespace detail {

/// @brief Table name as a fixed_string, for use in NTTP contexts (e.g. references<>)
template <typename T>
consteval auto table_name_fs() {
  constexpr std::string_view sv = table_name_of<T>();
  fixed_string<sv.size() + 1> fs{};
  for (std::size_t i = 0; i < sv.size(); ++i) {
    fs.value[i] = sv[i];
  }
  return fs;
}

}  // namespace detail

/// @brief Adapter giving an annotated struct the static interface the query builder
/// expects from a table (TableConcept). Use via relx::t<Users>.
template <typename T>
struct table_t {
  using annotated_type = T;
  static constexpr std::string_view table_name = table_name_of<T>();
};

namespace detail {

/// @brief Build the schema::column specialization for annotated member M
template <std::meta::info M>
consteval std::meta::info make_column_type() {
  std::vector<std::meta::info> args;
  args.push_back(^^table_t<typename [:std::meta::parent_of(M):]>);
  args.push_back(std::meta::reflect_constant(member_name_fs<M>()));
  args.push_back(std::meta::type_of(M));
  for (std::meta::info a : std::meta::annotations_of(M)) {
    const std::meta::info mod_type = std::meta::remove_cv(std::meta::type_of(a));
    if (is_modifier_type(mod_type)) {
      args.push_back(mod_type);
    }
  }
  return std::meta::substitute(^^schema::column, args);
}

}  // namespace detail

/// @brief The schema::column type derived from annotated member M
template <std::meta::info M>
using column_for = typename [:detail::make_column_type<M>():];

namespace detail {

/// @brief Synthesizes (via define_aggregate) a struct whose members are column_for<M>
/// instances named after T's members, so relx::t<Users>.id is a real column object
template <typename T>
struct columns_holder {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(
             ^^T, std::meta::access_context::unchecked())) {
      specs.push_back(std::meta::data_member_spec(
          std::meta::substitute(^^column_for, {std::meta::reflect_constant(m)}),
          {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};

}  // namespace detail

/// @brief Table object for an annotated struct: one column member per field of T
/// (same names), plus the static interface the query builder expects. Use via relx::t<Users>:
///
/// ```cpp
/// constexpr auto users = relx::t<Users>;
/// auto q = relx::select(users.id, users.username).from(users).where(users.id == 42);
/// ```
template <typename T>
struct table_ref : detail::columns_holder<T>::type {
  using annotated_type = T;
  static constexpr std::string_view table_name = table_name_of<T>();
};

/// @brief Column modifier annotations
namespace ann {
inline constexpr schema::primary_key pk{};
inline constexpr schema::unique unique{};
inline constexpr schema::autoincrement autoincrement{};

/// @brief Foreign key annotation referencing an annotated table's member:
/// [[=relx::ann::fk<^^Users::id>]] int user_id;
template <std::meta::info M>
inline constexpr auto fk =
    references<detail::table_name_fs<typename [:std::meta::parent_of(M):]>(),
               detail::member_name_fs<M>()>{};
}  // namespace ann

/// @brief Column definitions for an annotated table, derived via reflection
template <typename T>
std::string collect_column_definitions(const table_t<T>&) {
  std::vector<std::string> defs;
  template for (constexpr std::meta::info m : refl::member_array<T>()) {
    defs.push_back(column_for<m>{}.sql_definition());
  }

  std::string result;
  for (std::size_t i = 0; i < defs.size(); ++i) {
    if (i > 0) {
      result += ",\n";
    }
    result += defs[i];
  }
  return result;
}

template <typename T>
std::string collect_column_definitions(const table_ref<T>&) {
  return collect_column_definitions(table_t<T>{});
}

/// @brief Annotated tables express constraints as column modifiers, so there are no
/// separate table-level constraint definitions
template <typename T>
std::string collect_constraint_definitions(const table_t<T>&) {
  return "";
}

template <typename T>
std::string collect_constraint_definitions(const table_ref<T>&) {
  return "";
}

// clang-format on

}  // namespace relx::schema

namespace relx {
using schema::column_for;
using schema::table;
using schema::table_name_of;
using schema::table_ref;
using schema::table_t;
namespace ann = schema::ann;

/// @brief Table object for queries: `constexpr auto users = relx::t<Users>;` then
/// `select(users.id).from(users).where(users.id == 42)`
template <typename T>
inline constexpr schema::table_ref<T> t{};

/// @brief Standalone column reference: relx::c<^^Users::id>. Equivalent to
/// relx::t<Users>.id; useful when a full table object is not wanted.
template <std::meta::info M>
inline constexpr schema::column_for<M> c{};
}  // namespace relx
