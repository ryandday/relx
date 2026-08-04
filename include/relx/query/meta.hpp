#pragma once

#include "../bind_param.hpp"
#include "../reflect.hpp"
#include "../schema/table.hpp"

#include <meta>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace relx::query {

namespace detail {

// clang-format off

template <schema::is_column T>
struct select_all_column_probe {};

/// @brief Data members of Table that are columns, in declaration order (constraint
/// members of classic tables are filtered out). Shared by select_all and returning_all.
template <typename Table>
consteval auto select_all_columns() {
  std::vector<std::meta::info> cols;
  for (std::meta::info m : refl::member_array<Table>()) {
    if (std::meta::can_substitute(^^select_all_column_probe, {std::meta::type_of(m)})) {
      cols.push_back(m);
    }
  }
  return std::define_static_array(cols);
}

// clang-format on

}  // namespace detail

/// @brief Helper to check if a tuple is empty
template <typename Tuple>
static constexpr bool is_empty_tuple() {
  return std::tuple_size_v<Tuple> == 0;
}

/// @brief Helper to convert a tuple of expressions to SQL
template <typename Tuple>
std::string tuple_to_sql(const Tuple& tuple, const char* separator) {
  std::stringstream ss;
  int i = 0;
  std::apply(
      [&](const auto&... items) { ((ss << (i++ > 0 ? separator : "") << items.to_sql()), ...); },
      tuple);
  return ss.str();
}

/// @brief Helper to collect bind parameters from a tuple of expressions
template <typename Tuple>
std::vector<bind_param> tuple_bind_params(const Tuple& tuple) {
  std::vector<bind_param> params;

  std::apply(
      [&](const auto&... items) {
        auto process_item = [&params](const auto& item) {
          auto item_params = item.bind_params();
          if (!item_params.empty()) {
            params.insert(params.end(), item_params.begin(), item_params.end());
          }
        };

        (process_item(items), ...);
      },
      tuple);

  return params;
}

/// @brief Helper to apply a function to each element of a tuple
template <typename Func, typename Tuple>
static void apply_tuple(Func&& func, const Tuple& tuple) {
  std::apply([&func](const auto&... args) { (func(args), ...); }, tuple);
}

/// @brief Helper to extract class type from a member pointer
template <typename T>
struct class_of_t;

template <typename Class, typename T>
struct class_of_t<T Class::*> {
  using type = Class;
};

template <typename T>
using class_of_t_t = typename class_of_t<T>::type;

/// @brief Helper to extract column type from member pointer
template <auto MemberPtr>
struct column_type_of {
  using table_type = class_of_t_t<decltype(MemberPtr)>;
  using column_type = std::remove_reference_t<decltype(std::declval<table_type>().*MemberPtr)>;
};

/// @brief Helper to get column name from member pointer
template <auto MemberPtr>
constexpr auto column_name_of() {
  using column_t = typename column_type_of<MemberPtr>::column_type;
  return column_t::name;
}

}  // namespace relx::query