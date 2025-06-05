#pragma once

#include "core.hpp"
#include "fixed_string.hpp"

#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace relx::schema {

/// @brief UNIQUE constraint
struct unique {
  static std::string to_sql() { return " UNIQUE"; }
};

/// @brief PRIMARY KEY constraint
struct primary_key {
  static std::string to_sql() { return " PRIMARY KEY"; }
};

/// @brief AUTOINCREMENT constraint
struct autoincrement {
  static std::string to_sql() {
    // SQLite syntax
    return " AUTOINCREMENT";
  }
};

/// @brief Concept for valid identity types
template <typename T>
concept ValidIdentityType = std::is_integral_v<T> && !std::is_same_v<T, bool>;

/// @brief IDENTITY constraint with configurable options
/// @tparam Start The starting value for the identity column
/// @tparam Increment The increment value for the identity column
/// @tparam MinValue The minimum value for the identity column
/// @tparam MaxValue The maximum value for the identity column
/// @tparam Cycle Whether to cycle back to MinValue when MaxValue is reached
template <auto Start = 1, auto Increment = 1,
          auto MinValue = std::numeric_limits<decltype(Start)>::min(),
          auto MaxValue = std::numeric_limits<decltype(Start)>::max(), bool Cycle = false>
  requires ValidIdentityType<decltype(Start)> && ValidIdentityType<decltype(Increment)> &&
           ValidIdentityType<decltype(MinValue)> && ValidIdentityType<decltype(MaxValue)>
struct identity {
  static constexpr auto start = Start;
  static constexpr auto increment = Increment;
  static constexpr auto min_value = MinValue;
  static constexpr auto max_value = MaxValue;
  static constexpr bool cycle = Cycle;

  static std::string to_sql() {
    std::string result = " GENERATED ALWAYS AS IDENTITY";

    // Add options if they differ from defaults
    if constexpr (start != 1 || increment != 1 ||
                  min_value != std::numeric_limits<decltype(start)>::min() ||
                  max_value != std::numeric_limits<decltype(start)>::max() || cycle) {
      result += " (";

      // Add start value if not default
      if constexpr (start != 1) {
        result += "START WITH " + std::to_string(start);
      }

      // Add increment if not default
      if constexpr (increment != 1) {
        if constexpr (start != 1) {
          result += " ";
        }
        result += "INCREMENT BY " + std::to_string(increment);
      }

      // Add min value if not default
      if constexpr (min_value != std::numeric_limits<decltype(start)>::min()) {
        if constexpr (start != 1 || increment != 1) {
          result += " ";
        }
        result += "MINVALUE " + std::to_string(min_value);
      }

      // Add max value if not default
      if constexpr (max_value != std::numeric_limits<decltype(start)>::max()) {
        if constexpr (start != 1 || increment != 1 ||
                      min_value != std::numeric_limits<decltype(start)>::min()) {
          result += " ";
        }
        result += "MAXVALUE " + std::to_string(max_value);
      }

      // Add cycle option if true
      if constexpr (cycle) {
        if constexpr (start != 1 || increment != 1 ||
                      min_value != std::numeric_limits<decltype(start)>::min() ||
                      max_value != std::numeric_limits<decltype(start)>::max()) {
          result += " ";
        }
        result += "CYCLE";
      }

      result += ")";
    }

    return result;
  }
};

/// @brief CHECK constraint
template <FixedString Expr>
struct check {
  static constexpr auto expr = Expr;

  static std::string to_sql() { return " CHECK(" + std::string(std::string_view(expr)) + ")"; }
};

/// @brief REFERENCES constraint for foreign keys
template <FixedString Table, FixedString Column>
struct references {
  static constexpr auto table = Table;
  static constexpr auto column = Column;

  static std::string to_sql() {
    return " REFERENCES " + std::string(std::string_view(table)) + "(" +
           std::string(std::string_view(column)) + ")";
  }
};

/// @brief ON DELETE action for foreign keys
template <FixedString Action>
struct on_delete {
  static constexpr auto action = Action;

  static std::string to_sql() { return " ON DELETE " + std::string(std::string_view(action)); }
};

/// @brief ON UPDATE action for foreign keys
template <FixedString Action>
struct on_update {
  static constexpr auto action = Action;

  static std::string to_sql() { return " ON UPDATE " + std::string(std::string_view(action)); }
};

/// @brief DEFAULT value for non-string values
template <auto Value>
struct default_value {
  using value_type = decltype(Value);

  static constexpr value_type value = Value;

  static std::string to_sql() {
    if constexpr (std::is_same_v<value_type, bool>) {
      // For boolean values, use true/false in SQL
      return " DEFAULT " + std::string(value ? "true" : "false");
    } else if constexpr (std::is_integral_v<value_type> || std::is_floating_point_v<value_type>) {
      // For numeric types, convert to string
      return " DEFAULT " + std::to_string(value);
    } else {
      // Fall back to generic string conversion
      return " DEFAULT " + std::to_string(value);
    }
  }
};

/// @brief DEFAULT value for string literals
template <FixedString Value, bool IsLiteral = false>
struct string_default {
  static constexpr auto value = Value;

  static std::string to_sql() {
    if constexpr (IsLiteral) {
      // SQL function or literal that should not be quoted
      return " DEFAULT " + std::string(std::string_view(value));
    } else {
      // Normal string that should be quoted
      return " DEFAULT '" + std::string(std::string_view(value)) + "'";
    }
  }
};

/// @brief NULL default for optional types
struct null_default {
  static std::string to_sql() { return " DEFAULT NULL"; }
};

/// @brief Helper to apply all column modifiers to a SQL definition
template <typename... Modifiers>
static std::string apply_modifiers() {
  std::string result;
  [[maybe_unused]] auto _ = (result += ... += Modifiers::to_sql());  // supress unused warning
  return result;
}

// Type trait to detect default_value specialization
template <typename TypeParam>
struct is_default_value_specialization : std::false_type {};

template <auto Value>
struct is_default_value_specialization<default_value<Value>> : std::true_type {
  using value_type = decltype(Value);
};

// Type trait to detect string_default specialization
template <typename TypeParam>
struct is_string_default_specialization : std::false_type {};

template <FixedString Value, bool IsLiteral>
struct is_string_default_specialization<string_default<Value, IsLiteral>> : std::true_type {};

/// @brief Represents a column in a database table
/// @tparam TableT The table type this column belongs to
/// @tparam Name The name of the column as a string literal
/// @tparam T The C++ type of the column
/// @tparam Modifiers Additional column modifiers (UNIQUE, PRIMARY KEY, etc.)
template <typename TableT, FixedString Name, typename T, typename... Modifiers>
class column {
public:
  /// @brief The table type this column belongs to
  using table_type = TableT;

  /// @brief The C++ type of the column
  using value_type = T;

  /// @brief The column name
  static constexpr auto name = Name;

  /// @brief The SQL type of the column
  static constexpr auto sql_type = column_traits<T>::sql_type_name;

  /// @brief Flag indicating if the column can be NULL
  static constexpr bool nullable = column_traits<T>::nullable;

  /// @brief Get the SQL definition of this column
  /// @return A string containing the SQL column definition
  std::string sql_definition() const {
    std::string result = std::string(std::string_view(name)) + " " +
                         std::string(std::string_view(sql_type));

    // Add NOT NULL if not nullable
    if (!nullable) {
      result += " NOT NULL";
    }

    // Apply all modifiers
    result += apply_modifiers<Modifiers...>();

    return result;
  }

  /// @brief Convert a C++ value to SQL string
  /// @param value The value to convert
  /// @return SQL string representation
  static std::string to_sql_string(const T& value) {
    return column_traits<T>::to_sql_string(value);
  }

  /// @brief Parse a SQL string to C++ value
  /// @param sql_str The SQL string to parse
  /// @return The C++ value
  static T from_sql_string(const std::string& sql_str) {
    return column_traits<T>::from_sql_string(sql_str);
  }

  /// @brief Get the default value if set
  /// @return Optional containing the default value or empty if not set
  std::optional<T> get_default_value() const { return find_default_value<T, Modifiers...>(); }

  /// @brief Create a LIKE condition for this column
  /// @param pattern The pattern to match against
  /// @return A LIKE condition expression
  template <typename PatternType>
    requires std::convertible_to<PatternType, std::string>
  auto like(PatternType&& pattern) const;

  /// @brief Create an IS NULL condition for this column
  /// @return An IS NULL condition expression
  auto is_null() const;

  /// @brief Create an IS NOT NULL condition for this column
  /// @return An IS NOT NULL condition expression
  auto is_not_null() const;

private:
  // Helper to find default value in the modifiers
  template <typename ValueT, typename First, typename... Rest>
  static std::optional<ValueT> find_default_value() {
    if constexpr (is_default_value_type<First, ValueT>()) {
      return extract_default_value<First, ValueT>();
    } else if constexpr (sizeof...(Rest) > 0) {
      return find_default_value<ValueT, Rest...>();
    } else {
      return std::nullopt;
    }
  }

  template <typename ValueT>
  static std::optional<ValueT> find_default_value() {
    return std::nullopt;
  }

  // Check if a type is a default value type
  template <typename Mod, typename ValueT>
  static constexpr bool is_default_value_type() {
    if constexpr (std::is_same_v<Mod, null_default>) {
      return column_traits<ValueT>::nullable;
    } else if constexpr (is_default_value_specialization<Mod>::value) {
      using DefaultValueType = typename is_default_value_specialization<Mod>::value_type;
      return std::is_same_v<DefaultValueType, ValueT> ||
             std::is_convertible_v<DefaultValueType, ValueT>;
    } else if constexpr (is_string_default_specialization<Mod>::value) {
      if constexpr (std::is_same_v<ValueT, std::string> ||
                    std::is_convertible_v<std::string, ValueT> ||
                    std::is_constructible_v<ValueT, std::string_view> ||
                    std::is_constructible_v<ValueT, const char*>) {
        return true;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  // Extract the default value from a modifier
  template <typename Mod, typename ValueT>
  static std::optional<ValueT> extract_default_value() {
    if constexpr (std::is_same_v<Mod, null_default>) {
      if constexpr (column_traits<ValueT>::nullable) {
        return std::nullopt;
      }
    } else if constexpr (is_default_value_specialization<Mod>::value) {
      return static_cast<ValueT>(Mod::value);
    } else if constexpr (is_string_default_specialization<Mod>::value) {
      if constexpr (std::is_constructible_v<ValueT, std::string_view>) {
        return ValueT(std::string_view(Mod::value));
      } else if constexpr (std::is_constructible_v<ValueT, std::string>) {
        return ValueT(std::string(std::string_view(Mod::value)));
      } else if constexpr (std::is_constructible_v<ValueT, const char*>) {
        return ValueT(Mod::value.data());
      }
    }
    return std::nullopt;
  }
};

// Specialization for std::optional columns (nullable)
template <typename TableT, FixedString Name, typename T, typename... Modifiers>
class column<TableT, Name, std::optional<T>, Modifiers...> {
public:
  /// @brief The table type this column belongs to
  using table_type = TableT;

  using value_type = std::optional<T>;
  using base_type = T;

  static constexpr auto name = Name;
  static constexpr auto sql_type = column_traits<T>::sql_type_name;
  static constexpr bool nullable = true;

  std::string sql_definition() const {
    std::string result = std::string(std::string_view(name)) + " " +
                         std::string(std::string_view(sql_type));

    // No NOT NULL constraint for optional columns

    // Apply all modifiers
    result += apply_modifiers<Modifiers...>();

    return result;
  }

  static std::string to_sql_string(const std::optional<T>& value) {
    if (value.has_value()) {
      return column_traits<T>::to_sql_string(*value);
    }
    return "NULL";
  }

  static std::optional<T> from_sql_string(const std::string& sql_str) {
    if (sql_str == "NULL") {
      return std::nullopt;
    }
    return column_traits<T>::from_sql_string(sql_str);
  }

  /// @brief Get the default value if set
  /// @return Optional containing the default value or empty if not set
  std::optional<std::optional<T>> get_default_value() const {
    return find_default_value<std::optional<T>, Modifiers...>();
  }

  /// @brief Create a LIKE condition for this column
  /// @param pattern The pattern to match against
  /// @return A LIKE condition expression
  template <typename PatternType>
    requires std::convertible_to<PatternType, std::string>
  auto like(PatternType&& pattern) const;

  /// @brief Create an IS NULL condition for this column
  /// @return An IS NULL condition expression
  auto is_null() const;

  /// @brief Create an IS NOT NULL condition for this column
  /// @return An IS NOT NULL condition expression
  auto is_not_null() const;

private:
  // Helper to find default value in the modifiers
  template <typename ValueT, typename First, typename... Rest>
  static std::optional<ValueT> find_default_value() {
    if constexpr (is_default_value_type<First, ValueT>()) {
      return extract_default_value<First, ValueT>();
    } else if constexpr (sizeof...(Rest) > 0) {
      return find_default_value<ValueT, Rest...>();
    } else {
      return std::nullopt;
    }
  }

  template <typename ValueT>
  static std::optional<ValueT> find_default_value() {
    return std::nullopt;
  }

  // Check if a type is a default value type
  template <typename Mod, typename ValueT>
  static constexpr bool is_default_value_type() {
    if constexpr (std::is_same_v<Mod, null_default>) {
      return true;  // null_default is valid for std::optional<T>
    } else if constexpr (is_default_value_specialization<Mod>::value) {
      using DefaultValueType = typename is_default_value_specialization<Mod>::value_type;
      using OptionalBaseType = typename ValueT::value_type;
      return std::is_same_v<DefaultValueType, OptionalBaseType> ||
             std::is_convertible_v<DefaultValueType, OptionalBaseType>;
    } else if constexpr (is_string_default_specialization<Mod>::value) {
      using OptionalBaseType = typename ValueT::value_type;
      return std::is_convertible_v<std::string_view, OptionalBaseType>;
    } else {
      return false;
    }
  }

  // Extract the default value from a modifier
  template <typename Mod, typename ValueT>
  static std::optional<ValueT> extract_default_value() {
    if constexpr (std::is_same_v<Mod, null_default>) {
      if constexpr (column_traits<ValueT>::nullable) {
        return std::nullopt;
      }
    } else if constexpr (is_default_value_specialization<Mod>::value) {
      return static_cast<ValueT>(Mod::value);
    } else if constexpr (is_string_default_specialization<Mod>::value) {
      if constexpr (std::is_constructible_v<ValueT, std::string_view>) {
        return ValueT(std::string_view(Mod::value));
      } else if constexpr (std::is_constructible_v<ValueT, std::string>) {
        return ValueT(std::string(std::string_view(Mod::value)));
      } else if constexpr (std::is_constructible_v<ValueT, const char*>) {
        return ValueT(Mod::value.data());
      }
    }
    return std::nullopt;
  }
};

}  // namespace relx::schema
