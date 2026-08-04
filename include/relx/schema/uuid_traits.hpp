#pragma once

#include "core.hpp"

#include <stdexcept>
#include <string>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace relx::schema {

/// @brief Column traits for boost::uuids::uuid, mapped to PostgreSQL UUID.
/// to_sql_string produces a quoted SQL literal (DDL contexts); bind parameters
/// carry the raw text via the Value<boost::uuids::uuid> specialization.
template <>
struct column_traits<boost::uuids::uuid> {
  static constexpr auto sql_type_name = "UUID";
  static constexpr bool nullable = false;

  static std::string to_sql_string(const boost::uuids::uuid& value) {
    return "'" + boost::uuids::to_string(value) + "'";
  }

  static boost::uuids::uuid from_sql_string(const std::string& value) {
    std::string clean = value;
    if (clean.size() >= 2 && clean.front() == '\'' && clean.back() == '\'') {
      clean = clean.substr(1, clean.size() - 2);
    }
    return boost::uuids::string_generator{}(clean);
  }
};

}  // namespace relx::schema
