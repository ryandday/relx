#pragma once

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace relx::connection {

/// @brief Convert a string value to the target type and assign it
/// @tparam T The target type
/// @param target The target variable
/// @param value The string value to convert
template <typename T>
void convert_and_assign(T& target, const std::string& value) {
  if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                std::is_same_v<T, char> || std::is_same_v<T, char16_t> ||
                std::is_same_v<T, char32_t>) {
    target = value;
  } else if constexpr (std::is_same_v<T, bool>) {
    // Handle PostgreSQL-style boolean values ('t', 'f', etc.)
    target = (value == "1" || value == "true" || value == "TRUE" || value == "True" ||
              value == "t" || value == "T" || value == "yes" || value == "YES" || value == "Y");
  } else if constexpr (std::is_integral_v<T>) {
    // // Handle empty strings
    // if (value.empty()) {
    //   target = 0;
    // } else {
    target = static_cast<T>(std::stoll(value));
    //   }
  } else if constexpr (std::is_floating_point_v<T>) {
    //   if (value.empty()) {
    //     target = 0.0;
    //   } else {
    target = std::stod(value);
    //   }
  } else {
    static_assert(std::is_same_v<T, bool>, "Unsupported type conversion");
    // needed
  }
}

/// @brief Helper function to perform the tuple assignment with index sequence
/// @tparam Tuple The tuple type
/// @tparam Indices Parameter pack for indices
/// @param tuple The tuple to assign to
/// @param row The data row
/// @param indices Index sequence for tuple elements
template <typename Tuple, size_t... Indices>
void apply_tuple_assignment(Tuple& tuple, const std::vector<std::string>& row,
                            std::index_sequence<Indices...>) {
  // For each index in the tuple, convert the string value to the appropriate type
  (convert_and_assign(std::get<Indices>(tuple), row[Indices]), ...);
}

/// @brief Helper function to map a result row to a tuple (and thus to a struct)
/// @tparam Tuple The tuple type that corresponds to the struct fields
/// @param tuple The tuple to fill with values
/// @param row The database result row containing values
template <typename Tuple>
void map_row_to_tuple(Tuple& tuple, const std::vector<std::string>& row) {
  // Apply tuple assignment using fold expressions and parameter pack expansion
  apply_tuple_assignment(
      tuple, row, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
}

}  // namespace relx::connection