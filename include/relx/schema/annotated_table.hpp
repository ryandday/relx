#pragma once

#include "../reflect.hpp"
#include "column.hpp"
#include "fixed_string.hpp"
#include "identifier.hpp"
#include "table.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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
/// Constraints that span columns are struct-level annotations (columns named as
/// strings, validated against the members at compile time):
///
/// ```cpp
/// struct [[=relx::table("order_items"),
///         =relx::ann::composite_pk("order_id", "product_id"),
///         =relx::ann::index_on("customer_id", "created_at"),
///         =relx::ann::check("quantity > 0")]] OrderItems { ... };
/// ```
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
/// expects from a table (TableConcept). Use via relx::t<Users>, or relx::t<Users, "u">
/// for an aliased reference. When aliased, table_name IS the alias - every column
/// qualification and nested-row member name uses it - while base_table_name keeps the
/// real table for FROM/JOIN rendering ("users AS u").
template <typename T, fixed_string Alias = "">
struct table_t {
  using annotated_type = T;
  static constexpr std::string_view base_table_name = table_name_of<T>();
  static constexpr std::string_view alias_name = std::string_view(Alias);
  static constexpr bool is_aliased = !alias_name.empty();
  static constexpr std::string_view table_name = is_aliased ? alias_name : base_table_name;
};

namespace detail {

/// @brief Build the schema::column specialization for annotated member M, bound to
/// table_t<Parent, Alias> so aliased references qualify columns with the alias
template <std::meta::info M, fixed_string Alias>
consteval std::meta::info make_column_type() {
  std::vector<std::meta::info> args;
  args.push_back(std::meta::substitute(
      ^^table_t, {std::meta::parent_of(M), std::meta::reflect_constant(Alias)}));
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
template <std::meta::info M, fixed_string Alias = "">
using column_for = typename [:detail::make_column_type<M, Alias>():];

namespace detail {

/// @brief Synthesizes (via define_aggregate) a struct whose members are column_for<M>
/// instances named after T's members, so relx::t<Users>.id is a real column object.
/// Each alias gets its own synthesized type - two aliases of one table are distinct
/// types, which is what makes self-joins expressible.
template <typename T, fixed_string Alias>
struct columns_holder {
  struct type;
  consteval {
    std::vector<std::meta::info> specs;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(
             ^^T, std::meta::access_context::unchecked())) {
      specs.push_back(std::meta::data_member_spec(
          std::meta::substitute(^^column_for, {std::meta::reflect_constant(m),
                                               std::meta::reflect_constant(Alias)}),
          {.name = std::meta::identifier_of(m)}));
    }
    std::meta::define_aggregate(^^type, specs);
  }
};

}  // namespace detail

/// @brief Table object for an annotated struct: one column member per field of T
/// (same names), plus the static interface the query builder expects. Use via
/// relx::t<Users>, or relx::t<Users, "u"> for an aliased reference (self-joins need
/// two distinct aliases):
///
/// ```cpp
/// constexpr auto users = relx::t<Users>;
/// constexpr auto managers = relx::t<Users, "m">;
/// auto q = relx::select(users.id, managers.username)
///              .from(users)
///              .join(managers, on(users.manager_id == managers.id));
/// ```
template <typename T, fixed_string Alias = "">
struct table_ref : detail::columns_holder<T, Alias>::type {
  using annotated_type = T;
  static constexpr std::string_view base_table_name = table_name_of<T>();
  static constexpr std::string_view alias_name = std::string_view(Alias);
  static constexpr bool is_aliased = !alias_name.empty();
  static constexpr std::string_view table_name = is_aliased ? alias_name : base_table_name;
};

namespace detail {

/// @brief Fixed-capacity list of column names for struct-level annotations.
/// Annotation values must be structural, so names are stored in char arrays;
/// a name longer than max_len - 1 or more than max_cols names is a compile error
/// (out-of-bounds write in consteval).
struct name_list {
  static constexpr std::size_t max_cols = 8;
  static constexpr std::size_t max_len = 64;

  char names_[max_cols][max_len] = {};
  std::size_t lens_[max_cols] = {};
  std::size_t count_ = 0;

  consteval void add(const char* s) {
    std::size_t len = 0;
    while (s[len] != '\0') {
      names_[count_][len] = s[len];
      ++len;
    }
    lens_[count_] = len;
    ++count_;
  }

  constexpr std::string_view at(std::size_t i) const {
    return std::string_view(names_[i], lens_[i]);
  }

  constexpr std::string joined() const {
    std::string out;
    for (std::size_t i = 0; i < count_; ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += quote_identifier(at(i));
    }
    return out;
  }
};

}  // namespace detail

/// @brief Struct-level annotations for constraints that span columns. A struct-level
/// annotation cannot reference the struct's own members by reflection (the type is
/// incomplete at that point), so columns are named as strings; every name is validated
/// against the struct's members at compile time when DDL is generated.
///
/// ```cpp
/// struct [[=relx::table("order_items"),
///         =relx::ann::composite_pk("order_id", "product_id"),
///         =relx::ann::index_on("customer_id", "created_at"),
///         =relx::ann::check("quantity > 0")]] OrderItems { ... };
/// ```

/// @brief Composite PRIMARY KEY across the named columns
struct composite_pk : detail::name_list {
  static constexpr bool relx_table_constraint = true;
  static constexpr bool relx_is_primary_key = true;

  template <typename... Names>
  consteval explicit composite_pk(Names... names) {
    (add(names), ...);
  }

  consteval std::string constraint_sql() const { return "PRIMARY KEY (" + joined() + ")"; }
};

/// @brief Composite UNIQUE constraint across the named columns
struct composite_unique : detail::name_list {
  static constexpr bool relx_table_constraint = true;

  template <typename... Names>
  consteval explicit composite_unique(Names... names) {
    (add(names), ...);
  }

  consteval std::string constraint_sql() const { return "UNIQUE (" + joined() + ")"; }
};

/// @brief Composite FOREIGN KEY: local columns are named as strings, target columns by
/// reflection (the target table is already complete):
/// [[=relx::ann::composite_fk<^^Orders::region, ^^Orders::code>("order_region", "order_code")]]
template <std::meta::info... Targets>
  requires (sizeof...(Targets) > 0)
struct composite_fk : detail::name_list {
  static constexpr bool relx_table_constraint = true;
  static constexpr bool relx_is_composite_fk = true;

  template <typename... Names>
  consteval explicit composite_fk(Names... names) {
    static_assert(sizeof...(Names) == sizeof...(Targets),
                  "composite_fk: one local column name per referenced target column");
    (add(names), ...);
  }

  consteval std::string constraint_sql() const {
    constexpr std::meta::info targets[] = {Targets...};
    std::string out = "FOREIGN KEY (" + joined() + ") REFERENCES ";
    out += quote_identifier(table_name_of<typename [:std::meta::parent_of(targets[0]):]>());
    out += " (";
    bool first = true;
    for (std::meta::info target : targets) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += quote_identifier(std::meta::identifier_of(target));
    }
    out += ")";
    return out;
  }

  /// @brief Problems with the referenced columns, empty when valid: all targets must
  /// belong to one table, and they must form that table's primary key or a declared
  /// unique set (the database enforces the same rule at CREATE time; catching it here
  /// turns a runtime DDL failure into a compile error)
  static consteval std::string target_diagnostics() {
    std::string diag;
    constexpr std::meta::info targets[] = {Targets...};
    for (std::meta::info target : targets) {
      if (std::meta::parent_of(target) != std::meta::parent_of(targets[0])) {
        return "composite_fk target columns must all belong to one table; ";
      }
    }

    // The referenced names, for order-insensitive comparison against key sets
    constexpr std::size_t target_count = sizeof...(Targets);
    std::string_view target_names[target_count] = {std::meta::identifier_of(Targets)...};
    auto matches = [&](const name_list& names) {
      if (names.count_ != target_count) {
        return false;
      }
      for (std::string_view target_name : target_names) {
        bool found = false;
        for (std::size_t i = 0; i < names.count_; ++i) {
          found = found || names.at(i) == target_name;
        }
        if (!found) {
          return false;
        }
      }
      return true;
    };

    // Query annotations via the parent's info directly: reflecting a local alias
    // (^^Target after `using Target = ...`) designates the alias, whose annotation
    // list is empty
    bool covered = false;
    for (std::meta::info a : std::meta::annotations_of(std::meta::parent_of(targets[0]))) {
      const std::meta::info type = std::meta::remove_cv(std::meta::type_of(a));
      if (type == ^^composite_pk) {
        covered = covered || matches(std::meta::extract<composite_pk>(a));
      } else if (type == ^^composite_unique) {
        covered = covered || matches(std::meta::extract<composite_unique>(a));
      }
    }
    if (target_count == 1) {
      for (std::meta::info a : std::meta::annotations_of(targets[0])) {
        const std::meta::info type = std::meta::remove_cv(std::meta::type_of(a));
        covered = covered || type == ^^primary_key || type == ^^unique;
      }
    }
    if (!covered) {
      diag += "composite_fk must reference the target table's primary key or a declared "
              "unique column set; ";
    }
    return diag;
  }
};

/// @brief Table-level CHECK constraint with a raw SQL condition; optionally named via
/// [[=relx::ann::check("quantity > 0").named("positive_quantity")]].
/// (Named annotated_check because schema::check is the column-modifier CHECK; the
/// ann::check alias is the intended spelling.)
struct annotated_check {
  static constexpr bool relx_table_constraint = true;
  static constexpr std::size_t max_cond_len = 256;
  static constexpr std::size_t max_name_len = 64;

  char cond_[max_cond_len] = {};
  std::size_t cond_len_ = 0;
  char name_[max_name_len] = {};
  std::size_t name_len_ = 0;

  consteval explicit annotated_check(const char* cond) {
    while (cond[cond_len_] != '\0') {
      cond_[cond_len_] = cond[cond_len_];
      ++cond_len_;
    }
  }

  consteval annotated_check named(const char* name) const {
    annotated_check copy = *this;
    copy.name_len_ = 0;
    while (name[copy.name_len_] != '\0') {
      copy.name_[copy.name_len_] = name[copy.name_len_];
      ++copy.name_len_;
    }
    return copy;
  }

  consteval std::string constraint_sql() const {
    std::string out;
    if (name_len_ > 0) {
      out += "CONSTRAINT " + std::string(std::string_view(name_, name_len_)) + " ";
    }
    out += "CHECK (" + std::string(std::string_view(cond_, cond_len_)) + ")";
    return out;
  }
};

/// @brief Index over the named columns; unique via
/// [[=relx::ann::index_on("email").unique()]]. Emitted as separate CREATE INDEX
/// statements - see relx::create_indexes_sql<T>().
struct index_on : detail::name_list {
  static constexpr bool relx_index_annotation = true;

  bool unique_ = false;

  template <typename... Names>
  consteval explicit index_on(Names... names) {
    (add(names), ...);
  }

  consteval index_on unique() const {
    index_on copy = *this;
    copy.unique_ = true;
    return copy;
  }

  consteval std::string index_name(std::string_view table) const {
    std::string name = std::string(table) + "_";
    for (std::size_t i = 0; i < count_; ++i) {
      name += at(i);
      name += "_";
    }
    name += "idx";
    return name;
  }

  /// @brief The statement body after CREATE, e.g. "UNIQUE INDEX t_a_idx ON t (a)"
  /// (migrations store this form; AddConstraintOperation prepends CREATE)
  consteval std::string index_sql_body(std::string_view table) const {
    std::string out = unique_ ? "UNIQUE " : "";
    out += "INDEX " + quote_identifier(index_name(table)) + " ON " + quote_identifier(table) + " (" +
           joined() + ")";
    return out;
  }

  consteval std::string index_sql(std::string_view table) const {
    return "CREATE " + index_sql_body(table);
  }
};

namespace detail {

template <typename A>
concept TableConstraintAnnotation = requires { requires A::relx_table_constraint; };

template <typename A>
concept IndexAnnotation = requires { requires A::relx_index_annotation; };

template <typename A>
concept NamedColumnList = std::derived_from<A, name_list>;

/// @brief (Named concept rather than an inline requires-expression: GCC 16 evaluates an
/// inline requires-expression inside a `template for` body once, not per iteration)
template <typename A>
concept CompositeFkAnnotation = requires { requires A::relx_is_composite_fk; };

/// @brief Problems with T's table-level annotations, empty when they are valid.
/// Collected as text so the static_assert message can name the offending columns.
template <typename T>
consteval std::string constraint_diagnostics() {
  std::string diag;

  bool member_level_pk = false;
  for (std::meta::info m : refl::member_array<T>()) {
    for (std::meta::info a : std::meta::annotations_of(m)) {
      if (std::meta::remove_cv(std::meta::type_of(a)) == ^^primary_key) {
        member_level_pk = true;
      }
    }
  }

  std::size_t composite_pk_count = 0;
  template for (constexpr std::meta::info a :
                std::define_static_array(std::meta::annotations_of(^^T))) {
    using A = typename [:std::meta::remove_cv(std::meta::type_of(a)):];
    if constexpr (NamedColumnList<A>) {
      auto v = std::meta::extract<A>(a);
      if (v.count_ == 0) {
        diag += "annotation must name at least one column; ";
      }
      for (std::size_t i = 0; i < v.count_; ++i) {
        if (!refl::has_field_named<T>(v.at(i))) {
          diag += "'" + std::string(v.at(i)) + "' is not a column of this table; ";
        }
      }
    }
    if constexpr (std::same_as<A, composite_pk>) {
      ++composite_pk_count;
    }
    if constexpr (CompositeFkAnnotation<A>) {
      diag += A::target_diagnostics();
    }
  }

  if (composite_pk_count > 1) {
    diag += "a table can have at most one composite_pk; ";
  }
  if (composite_pk_count > 0 && member_level_pk) {
    diag += "composite_pk conflicts with a column-level pk annotation; ";
  }
  return diag;
}

template <typename T>
consteval void validate_table_annotations() {
  static_assert(constraint_diagnostics<T>().empty(),
                std::string("invalid table-level annotations on '") +
                    std::string(refl::type_name<T>()) + "': " + constraint_diagnostics<T>());
}

/// @brief Table-level constraint clauses of T (comma/newline separated), from its
/// struct annotations
template <typename T>
consteval std::string table_constraint_defs() {
  validate_table_annotations<T>();
  std::string out;
  template for (constexpr std::meta::info a :
                std::define_static_array(std::meta::annotations_of(^^T))) {
    using A = typename [:std::meta::remove_cv(std::meta::type_of(a)):];
    if constexpr (TableConstraintAnnotation<A>) {
      if (!out.empty()) {
        out += ",\n";
      }
      out += std::meta::extract<A>(a).constraint_sql();
    }
  }
  return out;
}

template <typename T>
consteval std::size_t index_annotation_count() {
  std::size_t n = 0;
  template for (constexpr std::meta::info a :
                std::define_static_array(std::meta::annotations_of(^^T))) {
    if constexpr (IndexAnnotation<typename [:std::meta::remove_cv(std::meta::type_of(a)):]>) {
      ++n;
    }
  }
  return n;
}

}  // namespace detail

/// @brief CREATE INDEX statements for T's index_on annotations, one per annotation,
/// built at compile time into static storage
template <typename T>
consteval auto create_indexes_sql() {
  detail::validate_table_annotations<T>();
  std::array<std::string_view, detail::index_annotation_count<T>()> stmts{};
  std::size_t i = 0;
  template for (constexpr std::meta::info a :
                std::define_static_array(std::meta::annotations_of(^^T))) {
    using A = typename [:std::meta::remove_cv(std::meta::type_of(a)):];
    if constexpr (detail::IndexAnnotation<A>) {
      stmts[i++] = std::define_static_string(
          std::meta::extract<A>(a).index_sql(table_name_of<T>()));
    }
  }
  return stmts;
}

/// @brief Column modifier annotations
namespace ann {
inline constexpr schema::primary_key pk{};
inline constexpr schema::unique unique{};
inline constexpr schema::autoincrement autoincrement{};

/// @brief Store this enum column as a native database enum type (see
/// relx::create_enum_type_sql<E>() for the required CREATE TYPE)
inline constexpr schema::native_enum native_enum{};
/// PostgreSQL auto-assigned ids: GENERATED ALWAYS AS IDENTITY with configurable options
inline constexpr schema::identity<> identity{};

/// @brief Foreign key annotation referencing an annotated table's member:
/// [[=relx::ann::fk<^^Users::id>]] int user_id;
template <std::meta::info M>
inline constexpr auto fk =
    references<detail::table_name_fs<typename [:std::meta::parent_of(M):]>(),
               detail::member_name_fs<M>()>{};

// Struct-level constraint annotations
using check = schema::annotated_check;
using schema::composite_fk;
using schema::composite_pk;
using schema::composite_unique;
using schema::index_on;
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

/// @brief Table-level constraint definitions of an annotated table (composite keys,
/// composite uniques/FKs, checks), built at compile time from its struct annotations.
/// Single-column constraints remain column modifiers and appear in the column
/// definitions instead.
template <typename T>
consteval std::string_view table_constraints_sql() {
  return std::define_static_string(detail::table_constraint_defs<T>());
}

template <typename T>
std::string collect_constraint_definitions(const table_t<T>&) {
  constexpr std::string_view sql = table_constraints_sql<T>();
  return std::string(sql);
}

template <typename T>
std::string collect_constraint_definitions(const table_ref<T>&) {
  return collect_constraint_definitions(table_t<T>{});
}

// clang-format off

/// @brief Compile-time CREATE TABLE builder for an annotated table. Mirrors the runtime
/// create_table fluent interface, but to_sql() is consteval and returns a view of static
/// storage - the statement costs nothing at runtime:
///
/// ```cpp
/// constexpr auto ddl = relx::create_table_sql<Users>().if_not_exists().to_sql();
/// ```
///
/// Not available for tables with floating-point DEFAULT values (no constexpr
/// floating-point formatting); use the runtime create_table builder for those.
template <typename T>
struct create_table_sql_builder {
  bool if_not_exists_ = false;

  consteval create_table_sql_builder if_not_exists() const {
    create_table_sql_builder builder = *this;
    builder.if_not_exists_ = true;
    return builder;
  }

  consteval std::string_view to_sql() const {
    std::string sql = "CREATE TABLE ";
    if (if_not_exists_) {
      sql += "IF NOT EXISTS ";
    }
    sql += table_name_of<T>();
    sql += " (\n";

    bool first = true;
    template for (constexpr std::meta::info m : refl::member_array<T>()) {
      if (!first) {
        sql += ",\n";
      }
      first = false;
      sql += column_for<m>{}.sql_definition();
    }

    const std::string constraints = detail::table_constraint_defs<T>();
    if (!constraints.empty()) {
      sql += ",\n" + constraints;
    }

    sql += "\n);";
    return std::define_static_string(sql);
  }
};

template <typename T>
consteval create_table_sql_builder<T> create_table_sql() {
  return {};
}

/// @brief Compile-time DROP TABLE builder for an annotated table
template <typename T>
struct drop_table_sql_builder {
  bool if_exists_ = false;
  bool cascade_ = false;

  consteval drop_table_sql_builder if_exists() const {
    drop_table_sql_builder builder = *this;
    builder.if_exists_ = true;
    return builder;
  }

  consteval drop_table_sql_builder cascade() const {
    drop_table_sql_builder builder = *this;
    builder.cascade_ = true;
    return builder;
  }

  consteval std::string_view to_sql() const {
    std::string sql = "DROP TABLE ";
    if (if_exists_) {
      sql += "IF EXISTS ";
    }
    sql += table_name_of<T>();
    if (cascade_) {
      sql += " CASCADE";
    }
    sql += ";";
    return std::define_static_string(sql);
  }
};

template <typename T>
consteval drop_table_sql_builder<T> drop_table_sql() {
  return {};
}

// clang-format on

// clang-format on

}  // namespace relx::schema

namespace relx {
using schema::column_for;
using schema::create_indexes_sql;
using schema::create_table_sql;
using schema::drop_table_sql;
using schema::table;
using schema::table_name_of;
using schema::table_ref;
using schema::table_t;
namespace ann = schema::ann;

/// @brief Table object for queries: `constexpr auto users = relx::t<Users>;` then
/// `select(users.id).from(users).where(users.id == 42)`. An alias makes a distinct
/// table reference (`relx::t<Users, "m">`), rendering `users AS m` in FROM/JOIN and
/// qualifying its columns with `m.` - the way to express self-joins.
template <typename T, schema::fixed_string Alias = "">
inline constexpr schema::table_ref<T, Alias> t{};

/// @brief The synthesized table-object type of an annotated struct: the type of relx::t<T>
template <typename T>
using table_type = schema::table_ref<T>;

/// @brief Standalone column reference: relx::c<^^Users::id>. Equivalent to
/// relx::t<Users>.id; useful when a full table object is not wanted.
template <std::meta::info M>
inline constexpr schema::column_for<M> c{};
}  // namespace relx
