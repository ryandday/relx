#pragma once

#include "reflect.hpp"
#include "schema/core.hpp"

#include <array>
#include <charconv>
#include <expected>
#include <format>
#include <meta>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/// @brief Struct-typed JSONB columns: annotate a plain aggregate with
/// [[=relx::ann::jsonb]] and use it as a column type - encoding and decoding are
/// derived from the struct's fields via reflection.
///
/// ```cpp
/// struct [[=relx::ann::jsonb]] Metadata {
///   std::string device;
///   std::optional<std::string> note;
///   std::vector<int> tags;
/// };
///
/// struct [[=relx::table("events")]] Event {
///   [[=relx::ann::pk]] int id;
///   Metadata metadata;   // JSONB column
/// };
/// ```
///
/// Supported field types: bool, integers, floating point, std::string,
/// std::optional<T> (null), std::vector<T> (arrays), and nested aggregates.
/// Decoding is strict: unknown keys, missing non-optional keys, and malformed
/// payloads are errors, not silently-defaulted values.
namespace relx::json {

/// @brief Struct-level annotation marking an aggregate as a JSONB payload type
struct jsonb_marker {};

// clang-format off

namespace detail {

template <typename T>
inline constexpr bool is_optional_v = false;
template <typename U>
inline constexpr bool is_optional_v<std::optional<U>> = true;

template <typename T>
inline constexpr bool is_vector_v = false;
template <typename U>
inline constexpr bool is_vector_v<std::vector<U>> = true;

}  // namespace detail

template <typename T>
consteval bool is_jsonb_annotated() {
  for (std::meta::info a : std::meta::annotations_of(std::meta::dealias(^^T))) {
    if (std::meta::remove_cv(std::meta::type_of(a)) == ^^jsonb_marker) {
      return true;
    }
  }
  return false;
}

/// @brief Aggregates annotated with [[=relx::ann::jsonb]]
template <typename T>
concept JsonbAnnotated = std::is_class_v<T> && std::is_aggregate_v<T> &&
                         is_jsonb_annotated<T>();

namespace detail {

inline void encode_string(std::string& out, std::string_view s) {
  out += '"';
  for (const char c : s) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\b': out += "\\b"; break;
    case '\f': out += "\\f"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        out += std::format("\\u{:04x}", static_cast<unsigned char>(c));
      } else {
        out += c;
      }
    }
  }
  out += '"';
}

template <typename T>
void encode_value(std::string& out, const T& value) {
  using V = std::remove_cvref_t<T>;
  if constexpr (is_optional_v<V>) {
    if (value.has_value()) {
      encode_value(out, *value);
    } else {
      out += "null";
    }
  } else if constexpr (std::is_same_v<V, bool>) {
    out += value ? "true" : "false";
  } else if constexpr (std::is_integral_v<V> || std::is_floating_point_v<V>) {
    out += std::format("{}", value);
  } else if constexpr (std::is_same_v<V, std::string>) {
    encode_string(out, value);
  } else if constexpr (is_vector_v<V>) {
    out += '[';
    bool first = true;
    for (const auto& element : value) {
      if (!first) {
        out += ',';
      }
      first = false;
      encode_value(out, element);
    }
    out += ']';
  } else if constexpr (std::is_class_v<V> && std::is_aggregate_v<V>) {
    out += '{';
    bool first = true;
    template for (constexpr std::meta::info m : refl::member_array<V>()) {
      if (!first) {
        out += ',';
      }
      first = false;
      encode_string(out, std::meta::identifier_of(m));
      out += ':';
      encode_value(out, value.[:m:]);
    }
    out += '}';
  } else {
    static_assert(std::is_same_v<V, bool>,
                  "unsupported JSONB field type (use bool, integers, floating point, "
                  "std::string, std::optional, std::vector, or nested aggregates)");
  }
}

struct Parser {
  std::string_view text;
  std::size_t pos = 0;

  void skip_ws() {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' ||
                                 text[pos] == '\n' || text[pos] == '\r')) {
      ++pos;
    }
  }
  bool eof() const { return pos >= text.size(); }
  char peek() const { return text[pos]; }
  bool consume(char c) {
    skip_ws();
    if (eof() || text[pos] != c) {
      return false;
    }
    ++pos;
    return true;
  }
  bool literal(std::string_view lit) {
    if (text.substr(pos, lit.size()) == lit) {
      pos += lit.size();
      return true;
    }
    return false;
  }
};

template <typename T>
std::expected<T, std::string> parse_value(Parser& p);

inline std::expected<std::string, std::string> parse_string(Parser& p) {
  p.skip_ws();
  if (p.eof() || p.peek() != '"') {
    return std::unexpected("expected string");
  }
  ++p.pos;
  std::string out;
  while (!p.eof()) {
    const char c = p.text[p.pos++];
    if (c == '"') {
      return out;
    }
    if (c != '\\') {
      out += c;
      continue;
    }
    if (p.eof()) {
      break;
    }
    const char esc = p.text[p.pos++];
    switch (esc) {
    case '"': out += '"'; break;
    case '\\': out += '\\'; break;
    case '/': out += '/'; break;
    case 'b': out += '\b'; break;
    case 'f': out += '\f'; break;
    case 'n': out += '\n'; break;
    case 'r': out += '\r'; break;
    case 't': out += '\t'; break;
    case 'u': {
      const auto read_hex4 = [&](unsigned& code) {
        if (p.pos + 4 > p.text.size()) {
          return false;
        }
        auto [ptr, ec] =
            std::from_chars(p.text.data() + p.pos, p.text.data() + p.pos + 4, code, 16);
        if (ec != std::errc{} || ptr != p.text.data() + p.pos + 4) {
          return false;
        }
        p.pos += 4;
        return true;
      };
      unsigned code = 0;
      if (!read_hex4(code)) {
        return std::unexpected("bad \\u escape");
      }
      if (code >= 0xD800 && code <= 0xDBFF) {
        if (p.text.substr(p.pos, 2) != "\\u") {
          return std::unexpected("lone high surrogate");
        }
        p.pos += 2;
        unsigned low = 0;
        if (!read_hex4(low) || low < 0xDC00 || low > 0xDFFF) {
          return std::unexpected("bad surrogate pair");
        }
        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
      } else if (code >= 0xDC00 && code <= 0xDFFF) {
        return std::unexpected("lone low surrogate");
      }
      if (code < 0x80) {
        out += static_cast<char>(code);
      } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
      } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
      } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
      }
      break;
    }
    default:
      return std::unexpected("bad escape");
    }
  }
  return std::unexpected("unterminated string");
}

template <typename T>
std::expected<T, std::string> parse_object(Parser& p) {
  if (!p.consume('{')) {
    return std::unexpected("expected object");
  }
  T obj{};
  constexpr std::size_t field_count = refl::member_array<T>().size();
  std::array<bool, field_count> seen{};

  p.skip_ws();
  if (!p.consume('}')) {
    while (true) {
      auto key = parse_string(p);
      if (!key) {
        return std::unexpected(key.error());
      }
      if (!p.consume(':')) {
        return std::unexpected("expected ':' after key '" + *key + "'");
      }

      bool matched = false;
      std::string field_error;
      std::size_t index = 0;
      template for (constexpr std::meta::info m : refl::member_array<T>()) {
        if (!matched && field_error.empty() && *key == std::meta::identifier_of(m)) {
          matched = true;
          seen[index] = true;
          using F = typename [:std::meta::type_of(m):];
          auto value = parse_value<F>(p);
          if (!value) {
            field_error = "field '" + *key + "': " + value.error();
          } else {
            obj.[:m:] = std::move(*value);
          }
        }
        ++index;
      }
      if (!field_error.empty()) {
        return std::unexpected(field_error);
      }
      if (!matched) {
        return std::unexpected("unknown key '" + *key + "'");
      }

      p.skip_ws();
      if (p.consume(',')) {
        continue;
      }
      if (p.consume('}')) {
        break;
      }
      return std::unexpected("expected ',' or '}'");
    }
  }

  std::string missing;
  std::size_t index = 0;
  template for (constexpr std::meta::info m : refl::member_array<T>()) {
    using F = typename [:std::meta::type_of(m):];
    if (!seen[index] && !is_optional_v<F> && missing.empty()) {
      missing = std::string(std::meta::identifier_of(m));
    }
    ++index;
  }
  if (!missing.empty()) {
    return std::unexpected("missing key '" + missing + "'");
  }
  return obj;
}

template <typename T>
std::expected<T, std::string> parse_value(Parser& p) {
  using V = std::remove_cvref_t<T>;
  p.skip_ws();
  if (p.eof()) {
    return std::unexpected("unexpected end of input");
  }

  if constexpr (is_optional_v<V>) {
    if (p.literal("null")) {
      return V{std::nullopt};
    }
    auto inner = parse_value<typename V::value_type>(p);
    if (!inner) {
      return std::unexpected(inner.error());
    }
    return V{std::move(*inner)};
  } else if constexpr (std::is_same_v<V, bool>) {
    if (p.literal("true")) {
      return true;
    }
    if (p.literal("false")) {
      return false;
    }
    return std::unexpected("expected boolean");
  } else if constexpr (std::is_integral_v<V> || std::is_floating_point_v<V>) {
    V value{};
    auto [ptr, ec] =
        std::from_chars(p.text.data() + p.pos, p.text.data() + p.text.size(), value);
    if (ec != std::errc{}) {
      return std::unexpected("bad number");
    }
    p.pos = static_cast<std::size_t>(ptr - p.text.data());
    return value;
  } else if constexpr (std::is_same_v<V, std::string>) {
    return parse_string(p);
  } else if constexpr (is_vector_v<V>) {
    if (!p.consume('[')) {
      return std::unexpected("expected array");
    }
    V out;
    p.skip_ws();
    if (p.consume(']')) {
      return out;
    }
    while (true) {
      auto element = parse_value<typename V::value_type>(p);
      if (!element) {
        return std::unexpected(element.error());
      }
      out.push_back(std::move(*element));
      p.skip_ws();
      if (p.consume(',')) {
        continue;
      }
      if (p.consume(']')) {
        return out;
      }
      return std::unexpected("expected ',' or ']'");
    }
  } else if constexpr (std::is_class_v<V> && std::is_aggregate_v<V>) {
    return parse_object<V>(p);
  } else {
    static_assert(std::is_same_v<V, bool>,
                  "unsupported JSONB field type (use bool, integers, floating point, "
                  "std::string, std::optional, std::vector, or nested aggregates)");
  }
}

}  // namespace detail

/// @brief Encode a value as compact JSON text
template <typename T>
std::string to_json(const T& value) {
  std::string out;
  detail::encode_value(out, value);
  return out;
}

/// @brief Parse JSON text into T. Strict: unknown keys, missing non-optional keys,
/// malformed input, and trailing characters are errors.
template <typename T>
std::expected<T, std::string> from_json(std::string_view text) {
  detail::Parser p{.text = text};
  auto value = detail::parse_value<T>(p);
  if (!value) {
    return value;
  }
  p.skip_ws();
  if (!p.eof()) {
    return std::unexpected("trailing characters after JSON value");
  }
  return value;
}

// clang-format on

}  // namespace relx::json

namespace relx::schema::ann {
/// @brief Marks an aggregate as a JSONB payload type: [[=relx::ann::jsonb]]
inline constexpr json::jsonb_marker jsonb{};
}  // namespace relx::schema::ann

namespace relx::schema {

/// @brief Column traits for JSONB-annotated aggregates: the column stores the
/// reflection-derived JSON encoding. The bind/text form is the raw JSON document
/// (no SQL quoting - parameters travel out-of-band).
template <json::JsonbAnnotated T>
struct column_traits<T> {
  static constexpr auto sql_type_name = "JSONB";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const T& value) { return json::to_json(value); }

  static T from_sql_string(const std::string& text) {
    auto parsed = json::from_json<T>(text);
    if (!parsed) {
      throw std::runtime_error("invalid JSONB payload: " + parsed.error());
    }
    return *parsed;
  }
};

}  // namespace relx::schema
