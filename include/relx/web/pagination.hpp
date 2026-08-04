#pragma once

#include "error.hpp"
#include "router.hpp"

#include <charconv>
#include <cstddef>
#include <string_view>

namespace relx::web {

/// @brief Limit/offset pagination parsed from ?limit= and ?offset= query params
struct Page {
  int limit;
  int offset;
};

/// @brief Parse pagination from the request's query string, with bounds.
/// Absent params fall back to defaults; non-numeric or out-of-range values are a 400.
inline ApiResult<Page> page_params(const RequestContext& ctx, int default_limit = 50,
                                   int max_limit = 200) {
  auto parse = [](std::string_view s, int fallback) -> std::optional<int> {
    if (s.empty()) {
      return fallback;
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
      return std::nullopt;
    }
    return value;
  };

  auto limit = parse(ctx.query_param("limit"), default_limit);
  auto offset = parse(ctx.query_param("offset"), 0);
  if (!limit || !offset || *limit < 1 || *limit > max_limit || *offset < 0) {
    return std::unexpected(bad_request("invalid pagination parameters"));
  }
  return Page{.limit = *limit, .offset = *offset};
}

}  // namespace relx::web
