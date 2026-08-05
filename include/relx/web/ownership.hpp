#pragma once

#include "../connection/postgresql_connection.hpp"
#include "error.hpp"

#include <utility>

namespace relx::web {

/// @brief Map a relx connection error onto an HTTP-shaped ApiError.
/// Unique violations become 409; everything else is a 500.
inline ApiError from_connection_error(const relx::connection::ConnectionError& error) {
  if (error.message.find("duplicate key") != std::string::npos) {
    return conflict("resource already exists");
  }
  return {.status = 500, .message = error.message};
}

/// @brief Execute a select expected to match one row: 404 when it matches none.
/// The workhorse for GET /resource/:id handlers.
template <typename Row, typename Query>
ApiResult<Row> one_or_404(relx::PostgreSQLConnection& conn, const Query& query,
                          std::string_view what = "resource") {
  auto rows = conn.execute_many<Row>(query);
  if (!rows) {
    return std::unexpected(from_connection_error(rows.error()));
  }
  if (rows->empty()) {
    return std::unexpected(not_found(std::string(what) + " not found"));
  }
  return std::move(rows->front());
}

/// @brief Ownership guard for a fetched row: throws a 404 on mismatch — rather
/// than a 403, so resource existence is not leaked to non-owners. Pure guards
/// (require_*/validate_*) throw directly; the name announces the fallibility,
/// and there is no value for an unwrap to extract.
template <typename Id>
void require_owner(const Id& row_owner, const Id& current_user,
                   std::string_view what = "resource") {
  if (row_owner != current_user) {
    throw ApiException(not_found(std::string(what) + " not found"));
  }
}

}  // namespace relx::web
