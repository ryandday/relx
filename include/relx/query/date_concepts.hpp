#pragma once

#include "../schema/column.hpp"

#include <chrono>
#include <optional>
#include <type_traits>

/// @brief Type checking concepts for date/time operations
namespace relx::query::date_checking {

/// @brief Remove optional wrapper to get base type
template <typename T>
struct remove_optional {
  using type = T;
};

template <typename T>
struct remove_optional<std::optional<T>> {
  using type = T;
};

template <typename T>
using remove_optional_t = typename remove_optional<T>::type;

/// @brief Helper to check if a type is a time_point of any clock
template <typename T>
struct is_time_point : std::false_type {};

template <typename Clock, typename Duration>
struct is_time_point<std::chrono::time_point<Clock, Duration>> : std::true_type {};

template <typename T>
inline constexpr bool is_time_point_v = is_time_point<T>::value;

/// @brief Check if a type is a date/time type
template <typename T>
concept DateTimeType =
    is_time_point_v<remove_optional_t<std::remove_cvref_t<T>>> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>,
                 std::chrono::system_clock::time_point> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>,
                 std::chrono::time_point<std::chrono::system_clock>> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::year_month_day> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::year> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::month> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::day> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::weekday> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::year_month> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::month_day> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::duration<int64_t>> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::milliseconds> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::seconds> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::minutes> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::hours> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::days> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::weeks> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::months> ||
    std::same_as<remove_optional_t<std::remove_cvref_t<T>>, std::chrono::years>;

/// @brief Helper to extract column type for checking
template <typename T>
struct extract_column_type {
  using type = T;
};

template <typename TableT, schema::fixed_string Name, typename ColumnT, typename... Modifiers>
struct extract_column_type<schema::column<TableT, Name, ColumnT, Modifiers...>> {
  using type = ColumnT;
};

template <typename T>
using extract_column_type_t = typename extract_column_type<T>::type;

/// @brief Check if type is a date/time type (unwrapping optional)
template <typename T>
concept DateTimeColumn = DateTimeType<remove_optional_t<extract_column_type_t<T>>>;

}  // namespace relx::query::date_checking