#pragma once

/// @brief glaze JSON serialization for relx column types that are not native
/// JSON scalars: boost::uuids::uuid and std::chrono::system_clock::time_point
/// (ISO 8601 strings on the wire, matching the PostgreSQL text forms).

#include "../schema/chrono_traits.hpp"
#include "../schema/uuid_traits.hpp"

#include <chrono>
#include <string>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <glaze/glaze.hpp>

namespace glz {

template <>
struct from<JSON, boost::uuids::uuid> {
  template <auto Opts>
  static void op(boost::uuids::uuid& value, is_context auto&& ctx, auto&& it, auto&& end) {
    std::string str;
    parse<JSON>::op<Opts>(str, ctx, it, end);
    if (bool(ctx.error)) {
      return;
    }
    try {
      value = boost::uuids::string_generator{}(str);
    } catch (const std::exception&) {
      ctx.error = error_code::syntax_error;
    }
  }
};

template <>
struct to<JSON, boost::uuids::uuid> {
  template <auto Opts>
  static void op(const boost::uuids::uuid& value, is_context auto&& ctx, auto&& b,
                 auto&& ix) noexcept {
    serialize<JSON>::op<Opts>(boost::uuids::to_string(value), ctx, b, ix);
  }
};

template <>
struct from<JSON, std::chrono::system_clock::time_point> {
  template <auto Opts>
  static void op(std::chrono::system_clock::time_point& value, is_context auto&& ctx, auto&& it,
                 auto&& end) {
    std::string str;
    parse<JSON>::op<Opts>(str, ctx, it, end);
    if (bool(ctx.error)) {
      return;
    }
    try {
      value = relx::schema::column_traits<std::chrono::system_clock::time_point>::from_sql_string(
          str);
    } catch (const std::exception&) {
      ctx.error = error_code::syntax_error;
    }
  }
};

template <>
struct to<JSON, std::chrono::system_clock::time_point> {
  template <auto Opts>
  static void op(const std::chrono::system_clock::time_point& value, is_context auto&& ctx,
                 auto&& b, auto&& ix) noexcept {
    // The traits format an ISO 8601 SQL literal; strip its quotes for JSON
    std::string quoted =
        relx::schema::column_traits<std::chrono::system_clock::time_point>::to_sql_string(value);
    serialize<JSON>::op<Opts>(quoted.substr(1, quoted.size() - 2), ctx, b, ix);
  }
};

}  // namespace glz
