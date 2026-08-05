#pragma once

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/// @brief Typed bind parameters.
///
/// A bind_param carries the parameter's text form (the universal representation every
/// text-protocol path understands) plus, for types with a fixed-size PostgreSQL wire
/// encoding, the SQL kind and the exact big-endian binary bytes. Connections that speak
/// libpq's binary protocol send tagged parameters with their type OID in binary format
/// (no server-side text parsing, no float round-trip loss); untagged parameters are sent
/// as untyped text exactly as before, so the server still infers types from context.
namespace relx {

/// @brief SQL types relx binds in binary format. Values are the stable PostgreSQL
/// type OIDs from pg_type.h, so no libpq header is needed here.
enum class sql_kind : std::uint32_t {
  unspecified = 0,  ///< sent as untyped text; server infers the type
  boolean = 16,
  int8 = 20,
  int2 = 21,
  int4 = 23,
  float4 = 700,
  float8 = 701,
  date = 1082,
  timestamptz = 1184,
};

namespace detail {

/// PostgreSQL's epoch (2000-01-01) as offsets from the Unix epoch
inline constexpr std::chrono::sys_days pg_epoch_days{std::chrono::year{2000} / 1 / 1};

}  // namespace detail

/// @brief One query parameter: text form, optionally tagged with a SQL kind and the
/// corresponding binary wire encoding. Implicitly convertible from strings and
/// equality-comparable with them, so string-typed call sites keep working.
struct bind_param {
  std::string value;  ///< text form of the parameter
  sql_kind kind = sql_kind::unspecified;
  std::array<unsigned char, 8> binary{};  ///< big-endian wire bytes when kind is set
  std::uint8_t binary_size = 0;
  bool is_null = false;  ///< SQL NULL: sent as a null value, kind may still carry the type

  bind_param() = default;
  // NOLINTBEGIN(google-explicit-constructor): string call sites convert implicitly
  bind_param(std::string v) : value(std::move(v)) {}
  bind_param(std::string_view v) : value(v) {}
  bind_param(const char* v) : value(v) {}
  // NOLINTEND(google-explicit-constructor)

  friend bool operator==(const bind_param& a, const bind_param& b) = default;
  // Exact-match overloads per string type: with the implicit constructors above, a
  // single string_view overload would be ambiguous against member-wise comparison
  friend bool operator==(const bind_param& p, std::string_view text) { return p.value == text; }
  friend bool operator==(const bind_param& p, const std::string& text) { return p.value == text; }
  friend bool operator==(const bind_param& p, const char* text) { return p.value == text; }

  friend std::ostream& operator<<(std::ostream& os, const bind_param& p) { return os << p.value; }

  /// @brief A SQL NULL parameter. The kind (when known) keeps the parameter typed for
  /// the server; the text form "NULL" is only cosmetic (text-only paths cannot express
  /// NULL parameters - see to_text_params).
  static bind_param null(sql_kind kind = sql_kind::unspecified) {
    bind_param param;
    param.value = "NULL";
    param.kind = kind;
    param.is_null = true;
    return param;
  }
};

/// @brief The SQL kind a C++ type binds as: fixed-size wire-encodable types get their
/// kind, everything else is unspecified (untyped text)
template <typename T>
consteval sql_kind sql_kind_for() {
  if constexpr (std::is_same_v<T, bool>) {
    return sql_kind::boolean;
  } else if constexpr (std::is_integral_v<T>) {
    return sizeof(T) <= 2 ? sql_kind::int2 : sizeof(T) <= 4 ? sql_kind::int4 : sql_kind::int8;
  } else if constexpr (std::is_same_v<T, float>) {
    return sql_kind::float4;
  } else if constexpr (std::is_same_v<T, double>) {
    return sql_kind::float8;
  } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
    return sql_kind::timestamptz;
  } else if constexpr (std::is_same_v<T, std::chrono::year_month_day>) {
    return sql_kind::date;
  } else {
    return sql_kind::unspecified;
  }
}

namespace detail {

inline void store_big_endian(bind_param& param, std::uint64_t bits, std::uint8_t size) {
  param.binary_size = size;
  for (std::uint8_t i = 0; i < size; ++i) {
    param.binary[i] = static_cast<unsigned char>(bits >> (8 * (size - 1 - i)));
  }
}

}  // namespace detail

/// @brief Build a typed parameter from a C++ value and its text form. Types with a
/// fixed-size wire encoding (bool, integers, floats) are tagged with their SQL kind and
/// binary bytes; anything else stays untyped text.
template <typename T>
bind_param make_bind_param(const T& val, std::string text) {
  bind_param param;
  param.value = std::move(text);
  if constexpr (std::is_same_v<T, bool>) {
    param.kind = sql_kind::boolean;
    detail::store_big_endian(param, val ? 1 : 0, 1);
  } else if constexpr (std::is_integral_v<T>) {
    const auto wide = static_cast<std::int64_t>(val);
    if constexpr (sizeof(T) <= 2) {
      param.kind = sql_kind::int2;
      detail::store_big_endian(param, static_cast<std::uint16_t>(wide), 2);
    } else if constexpr (sizeof(T) <= 4) {
      param.kind = sql_kind::int4;
      detail::store_big_endian(param, static_cast<std::uint32_t>(wide), 4);
    } else {
      param.kind = sql_kind::int8;
      detail::store_big_endian(param, static_cast<std::uint64_t>(wide), 8);
    }
  } else if constexpr (std::is_same_v<T, float>) {
    param.kind = sql_kind::float4;
    detail::store_big_endian(param, std::bit_cast<std::uint32_t>(val), 4);
  } else if constexpr (std::is_same_v<T, double>) {
    param.kind = sql_kind::float8;
    detail::store_big_endian(param, std::bit_cast<std::uint64_t>(val), 8);
  } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
    // TIMESTAMPTZ wire format: microseconds since 2000-01-01 UTC
    param.kind = sql_kind::timestamptz;
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        val - std::chrono::sys_days{detail::pg_epoch_days});
    detail::store_big_endian(param, static_cast<std::uint64_t>(micros.count()), 8);
  } else if constexpr (std::is_same_v<T, std::chrono::year_month_day>) {
    // DATE wire format: days since 2000-01-01
    param.kind = sql_kind::date;
    const auto days = (std::chrono::sys_days{val} - detail::pg_epoch_days).count();
    detail::store_big_endian(param, static_cast<std::uint32_t>(static_cast<std::int32_t>(days)), 4);
  }
  return param;
}

/// @brief Strip type tags: the text forms only, for paths that speak the text protocol
inline std::vector<std::string> to_text_params(const std::vector<bind_param>& params) {
  std::vector<std::string> text;
  text.reserve(params.size());
  for (const bind_param& p : params) {
    text.push_back(p.value);
  }
  return text;
}

/// @brief Tag-free promotion of plain text parameters
inline std::vector<bind_param> to_bind_params(std::vector<std::string> params) {
  std::vector<bind_param> out;
  out.reserve(params.size());
  for (std::string& p : params) {
    out.emplace_back(std::move(p));
  }
  return out;
}

/// @brief Convenience comparison so tests can compare param lists against string lists
inline bool operator==(const std::vector<bind_param>& params,
                       const std::vector<std::string>& text) {
  if (params.size() != text.size()) {
    return false;
  }
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (params[i].value != text[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace relx
