#pragma once

#include "../reflect.hpp"
#include "../results/result.hpp"

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
template <typename T>
void convert_and_assign(T& target, const std::string& value) {
  if constexpr (detail::is_optional_v<T>) {
    typename T::value_type inner{};
    convert_and_assign(inner, value);
    target = std::move(inner);
  } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                       std::is_same_v<T, char> || std::is_same_v<T, char16_t> ||
                       std::is_same_v<T, char32_t>) {
    target = value;
  } else if constexpr (std::is_same_v<T, bool>) {
    // Handle PostgreSQL-style boolean values ('t', 'f', etc.)
    target = (value == "1" || value == "true" || value == "TRUE" || value == "True" ||
              value == "t" || value == "T" || value == "yes" || value == "YES" || value == "Y");
  } else if constexpr (std::is_integral_v<T>) {
    target = static_cast<T>(std::stoll(value));
  } else if constexpr (std::is_floating_point_v<T>) {
    target = std::stod(value);
  } else {
    static_assert(std::is_same_v<T, bool>, "Unsupported type conversion");
  }
}

/// @brief Map a result row onto an aggregate struct using reflection.
/// @details Each struct field is matched to a result column by name; when the field's
/// identifier does not appear in the result's column names (e.g. an unaliased expression
/// column), the field's position in the struct is used instead. NULL cells map to
/// std::nullopt for std::optional fields and are an error for any other field type.
/// @tparam T The aggregate struct to map onto
/// @param row The database result row
/// @return The mapped struct, or an error message
template <typename T>
std::expected<T, std::string> map_row_to_struct(const result::Row& row) {
  T obj{};
  std::string error;
  std::size_t field_index = 0;
  const auto& column_names = row.column_names();

  refl::for_each_named_field(obj, [&](auto& field, std::string_view name) {
    const std::size_t position = field_index++;
    if (!error.empty()) {
      return;
    }

    // Prefer the column whose name matches the field's identifier
    std::size_t column_index = position;
    for (std::size_t i = 0; i < column_names.size(); ++i) {
      if (column_names[i] == name) {
        column_index = i;
        break;
      }
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

    try {
      convert_and_assign(field, cell.raw_value());
    } catch (const std::exception& e) {
      error = "Failed to convert value for field '" + std::string(name) + "': " + e.what();
    }
  });

  if (!error.empty()) {
    return std::unexpected(error);
  }
  return obj;
}

}  // namespace relx::connection
