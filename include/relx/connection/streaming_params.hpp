#pragma once

#include "../query/core.hpp"

#include <string>
#include <vector>

namespace relx::connection {

/// @brief Text forms of a query object's bind parameters, in placeholder order
/// @details Streaming speaks libpq's text protocol, so only the text form of each
/// parameter travels; a parameter's binary encoding and SQL kind are dropped, and a
/// SQL NULL parameter is sent as the text "NULL" (the text protocol cannot express a
/// null parameter) exactly as the string-parameter overloads already do.
/// @param query Any query object exposing to_sql()/bind_params()
/// @return The parameter text values
template <query::SqlExpr Query>
std::vector<std::string> streaming_text_params(const Query& query) {
  const auto params = query.bind_params();
  std::vector<std::string> texts;
  texts.reserve(params.size());
  for (const auto& param : params) {
    texts.push_back(param.value);
  }
  return texts;
}

}  // namespace relx::connection
