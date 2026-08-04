#pragma once

#include "../schema/uuid_traits.hpp"
#include "value.hpp"

#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace relx::query {

/// @brief Specialization for boost::uuids::uuid values: bind the raw uuid text
/// (the traits' to_sql_string is SQL-literal syntax and must not leak into binds)
template <>
class Value<boost::uuids::uuid> : public SqlExpression {
public:
  using value_type = boost::uuids::uuid;

  explicit Value(boost::uuids::uuid value) : value_(value) {}

  std::string to_sql() const override { return "?"; }

  std::vector<bind_param> bind_params() const override {
    return {boost::uuids::to_string(value_)};
  }

  const boost::uuids::uuid& value() const { return value_; }

private:
  boost::uuids::uuid value_;
};

/// @brief Helper to create a value expression from a uuid
inline auto val(boost::uuids::uuid u) {
  return Value<boost::uuids::uuid>(u);
}

}  // namespace relx::query
