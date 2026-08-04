#pragma once

#include "../reflect.hpp"
#include "../results/result.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace relx::connection {

namespace detail {
template <typename T>
inline constexpr bool is_optional_v = false;
template <typename U>
inline constexpr bool is_optional_v<std::optional<U>> = true;
}  // namespace detail

/// @brief Convert a string value to the target type and assign it
/// @tparam T The target type
/// @param target The target variable
/// @param value The string value to convert
/// @return Empty expected on success, error message on conversion failure
template <typename T>
std::expected<void, std::string> convert_and_assign(T& target, const std::string& value) {
  if constexpr (detail::is_optional_v<T>) {
    typename T::value_type inner{};
    if (auto result = convert_and_assign(inner, value); !result) {
      return result;
    }
    target = std::move(inner);
    return {};
  } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                       std::is_same_v<T, char> || std::is_same_v<T, char16_t> ||
                       std::is_same_v<T, char32_t>) {
    target = value;
    return {};
  } else if constexpr (std::is_same_v<T, bool>) {
    // Handle PostgreSQL-style boolean values ('t', 'f', etc.)
    target = (value == "1" || value == "true" || value == "TRUE" || value == "True" ||
              value == "t" || value == "T" || value == "yes" || value == "YES" || value == "Y");
    return {};
  } else if constexpr (std::is_enum_v<T>) {
    auto parsed = refl::enum_cast<T>(value);
    if (!parsed) {
      return std::unexpected("'" + value + "' is not an enumerator of " +
                             std::string(refl::type_name<T>()));
    }
    target = *parsed;
    return {};
  } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), target);
    if (ec != std::errc{}) {
      return std::unexpected("'" + value + "' is not a valid " + std::string(refl::type_name<T>()));
    }
    return {};
  } else if constexpr (requires { schema::column_traits<T>::from_sql_string(value); }) {
    // Types with column traits (time_point, uuid, ...) parse via their trait
    try {
      target = schema::column_traits<T>::from_sql_string(value);
      return {};
    } catch (const std::exception& e) {
      return std::unexpected("'" + value + "' is not a valid " +
                             std::string(refl::type_name<T>()) + ": " + e.what());
    }
  } else {
    static_assert(std::is_same_v<T, bool>, "Unsupported type conversion");
  }
}

/// @brief Verify that every result column can land in some field of T. Runs once per
/// result set, not per row. With column names, matching is by name; without, positional
/// (so there must be at least as many fields as columns). A column with nowhere to land
/// is an error, since its data would be silently lost.
/// @details For typed queries this is compile-time-proven already
/// (assert_struct_covers_select_list) and only guards runtime drift such as PostgreSQL
/// case-folding unquoted identifiers; it is load-bearing for raw SQL and runtime aliases.
template <typename T>
std::expected<void, std::string> verify_result_columns_consumed(
    const std::vector<std::string>& column_names, std::size_t column_count) {
  static constexpr auto field_names = refl::field_names<T>();

  if (!column_names.empty()) {
    for (const auto& column : column_names) {
      if (std::find(field_names.begin(), field_names.end(), column) == field_names.end()) {
        return std::unexpected("result column '" + column + "' has no matching field in struct '" +
                               std::string(refl::type_name<T>()) +
                               "'; add the field or drop the column from the select");
      }
    }
  } else if (column_count > field_names.size()) {
    return std::unexpected("result has " + std::to_string(column_count) + " columns but struct '" +
                           std::string(refl::type_name<T>()) + "' has only " +
                           std::to_string(field_names.size()) + " field(s)");
  }
  return {};
}

/// @brief Map a result row onto an aggregate struct using reflection.
/// @details When the result carries column names, each struct field is matched to its
/// same-named column; fields with no matching column are left default-initialized (a
/// query selecting a subset of the struct's fields is valid - selecting expresses that
/// the other columns are not wanted). When the result has no column names, cells map
/// positionally. Callers are expected to run verify_result_columns_consumed<T> once per
/// result set beforehand. NULL cells map to std::nullopt for std::optional fields and
/// are an error otherwise.
/// @tparam T The aggregate struct to map onto
/// @param row The database result row
/// @return The mapped struct, or an error message
template <typename T>
std::expected<T, std::string> map_row_to_struct(const result::Row& row) {
  T obj{};
  std::string error;
  std::size_t field_index = 0;
  const auto& column_names = row.column_names();
  const bool by_name = !column_names.empty();

  refl::for_each_named_field(obj, [&](auto& field, std::string_view name) {
    const std::size_t position = field_index++;
    if (!error.empty()) {
      return;
    }

    // Resolve this field's column: by name when names are available, else by position
    std::size_t column_index = row.size();  // sentinel: no column for this field
    if (by_name) {
      for (std::size_t i = 0; i < column_names.size(); ++i) {
        if (column_names[i] == name) {
          column_index = i;
          break;
        }
      }
    } else if (position < row.size()) {
      column_index = position;
    }

    if (column_index == row.size()) {
      return;  // field not covered by the result - stays default-initialized
    }

    auto cell_result = row.get_cell(column_index);
    if (!cell_result) {
      error = "Failed to get cell for field '" + std::string(name) +
              "': " + cell_result.error().message;
      return;
    }
    const auto& cell = **cell_result;

    using FieldType = std::remove_cvref_t<decltype(field)>;
    if (cell.is_null()) {
      if constexpr (detail::is_optional_v<FieldType>) {
        field = std::nullopt;
      } else {
        error = "NULL value for non-optional field '" + std::string(name) + "'";
      }
      return;
    }

    if (auto converted = convert_and_assign(field, cell.raw_value()); !converted) {
      error = "Failed to convert value for field '" + std::string(name) + "': " + converted.error();
    }
  });

  if (!error.empty()) {
    return std::unexpected(error);
  }
  return obj;
}

}  // namespace relx::connection
