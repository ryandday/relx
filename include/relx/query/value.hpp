#pragma once

#include "core.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace relx::query {

/// @brief Represents a literal value in a SQL query
/// @tparam T The C++ type of the value
template <typename T>
class Value : public SqlExpression {
public:
  using value_type = T;

  explicit Value(T value) : value_(std::move(value)) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override {
    return {relx::make_bind_param(value_, schema::column_traits<T>::to_sql_string(value_))};
  }

  const T& value() const { return value_; }

private:
  T value_;
};

/// @brief Specialization for std::optional values. Disengaged optionals still render a
/// placeholder and bind a typed NULL parameter, so the SQL text does not depend on the
/// value (a value-independent statement can be prepared and reused).
template <typename T>
class Value<std::optional<T>> : public SqlExpression {
public:
  using value_type = std::optional<T>;

  explicit Value(std::optional<T> value) : value_(std::move(value)) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override {
    if (value_.has_value()) {
      // Bind exactly as the non-optional Value<T> would; to_sql_string is SQL-literal
      // syntax (quoted strings) and must not leak into bound parameter text
      return Value<T>(*value_).bind_params();
    }
    return {bind_param::null(relx::sql_kind_for<T>())};
  }

  const std::optional<T>& value() const { return value_; }

private:
  std::optional<T> value_;
};

/// @brief Specialization for boost::uuids::uuid values: typed binary bind (OID 2950,
/// the uuid's 16 bytes verbatim). The text form is the canonical string, not the
/// traits' SQL-literal syntax, which must not leak into binds.
template <>
class Value<boost::uuids::uuid> : public SqlExpression {
public:
  using value_type = boost::uuids::uuid;

  explicit Value(boost::uuids::uuid value) : value_(value) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override {
    bind_param param;
    param.value = boost::uuids::to_string(value_);
    param.kind = relx::sql_kind::uuid;
    param.binary_size = value_.size();  // 16: the wire format is the bytes verbatim
    std::copy(value_.begin(), value_.end(), param.binary.begin());
    return {param};
  }

  const boost::uuids::uuid& value() const { return value_; }

private:
  boost::uuids::uuid value_;
};

/// @brief Specialization for std::string values
template <>
class Value<std::string> : public SqlExpression {
public:
  using value_type = std::string;

  explicit Value(std::string value) : value_(std::move(value)) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override { return {value_}; }

  const std::string& value() const { return value_; }

private:
  std::string value_;
};

/// @brief Specialization for std::string_view values
template <>
class Value<std::string_view> : public SqlExpression {
public:
  using value_type = std::string_view;

  explicit Value(std::string_view value) : value_(value) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override { return {std::string(value_)}; }

  std::string_view value() const { return value_; }

private:
  std::string_view value_;
};

/// @brief Specialization for C-style string literals (const char*)
template <>
class Value<const char*> : public SqlExpression {
public:
  using value_type = const char*;

  explicit Value(const char* value) : value_(value) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override { return {std::string(value_)}; }

  const char* value() const { return value_; }

private:
  const char* value_;
};

/// @brief Create a value expression
/// @tparam T The value type
/// @param val The value
/// @return A Value expression
template <typename T>
auto value(T val) {
  return Value<T>(std::move(val));
}

/// @brief Helper to create a value expression from a string literal
/// @param str The string literal
/// @return A Value<std::string_view> expression
inline auto val(const char* str) {
  return Value<std::string_view>(str);
}

/// @brief Helper to create a value expression from a string
/// @param str The string
/// @return A Value<std::string> expression
inline auto val(std::string str) {
  return Value<std::string>(std::move(str));
}

/// @brief Helper to create a value expression from a string_view
/// @note Be careful with this one, if the underlying string_view goes out of scope, the value will
/// be invalid
/// @param sv The string_view
/// @return A Value<std::string_view> expression
inline auto val(std::string_view sv) {
  return Value<std::string_view>(sv);
}

/// @brief Helper to create a value expression from a uuid
/// @param u The uuid
/// @return A Value<boost::uuids::uuid> expression
inline auto val(boost::uuids::uuid u) {
  return Value<boost::uuids::uuid>(u);
}

/// @brief Helper to create a value expression from an int
/// @param i The integer
/// @return A Value<int> expression
inline auto val(int i) {
  return Value<int>(i);
}

/// @brief Helper to create a value expression from a long
/// @param l The long integer
/// @return A Value<long> expression
inline auto val(long l) {
  return Value<long>(l);
}

/// @brief Helper to create a value expression from a long long
/// @param ll The long long integer
/// @return A Value<long long> expression
inline auto val(long long ll) {
  return Value<long long>(ll);
}

/// @brief Helper to create a value expression from a double
/// @param d The double
/// @return A Value<double> expression
inline auto val(double d) {
  return Value<double>(d);
}

/// @brief Helper to create a value expression from a float
/// @param f The float
/// @return A Value<float> expression
inline auto val(float f) {
  return Value<float>(f);
}

/// @brief Helper to create a value expression from a bool
/// @param b The boolean
/// @return A Value<bool> expression
inline auto val(bool b) {
  return Value<bool>(b);
}

/// @brief Helper to create a value expression from an enum (bound as its
/// enumerator identifier)
/// @tparam E The enum type
/// @param e The enum value
/// @return A Value<E> expression
template <typename E>
  requires std::is_enum_v<E>
auto val(E e) {
  return Value<E>(e);
}

/// @brief Helper to create a value expression from an optional
/// @tparam T The optional value type
/// @param opt The optional value
/// @return A Value<std::optional<T>> expression
template <typename T>
auto val(std::optional<T> opt) {
  return Value<std::optional<T>>(std::move(opt));
}

/// @brief Specialization for a bare nullopt: an untyped NULL parameter
template <>
class Value<std::nullopt_t> : public SqlExpression {
public:
  using value_type = std::nullopt_t;

  explicit Value(std::nullopt_t) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override { return {bind_param::null()}; }
};

/// @brief Helper to create a value expression from a nullopt
/// @return A Value<std::nullopt_t> expression
inline auto val(std::nullopt_t) {
  return Value<std::nullopt_t>(std::nullopt);
}

}  // namespace relx::query