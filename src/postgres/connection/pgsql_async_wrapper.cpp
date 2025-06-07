#include "relx/connection/pgsql_async_wrapper.hpp"

#include "relx/connection/sql_utils.hpp"

#include <regex>

namespace relx::pgsql_async_wrapper {

// Helper function to convert ? placeholders to $1, $2, etc.
static std::string convert_placeholders(const std::string& sql) {
  return relx::connection::sql_utils::convert_placeholders_to_postgresql(sql);
}

// Implementation of PreparedStatement methods
boost::asio::awaitable<PgResult<void>> PreparedStatement::prepare() {
  if (prepared_) {
    co_return PgResult<void>{};
  }

  // Convert ? placeholders to $n format
  const std::string pg_query = convert_placeholders(query_);

  if (!PQsendPrepare(conn_.native_handle(), name_.c_str(), pg_query.c_str(), 0, nullptr)) {
    co_return std::unexpected(PgError::from_conn(conn_.native_handle()));
  }

  auto flush_result = co_await conn_.flush_outgoing_data();
  if (!flush_result) {
    co_return std::unexpected(flush_result.error());
  }

  auto res_result = co_await conn_.get_query_result();
  if (!res_result) {
    co_return std::unexpected(res_result.error());
  }

  if (!*res_result) {
    co_return std::unexpected(PgError{.message = res_result->error_message(),
                                      .error_code = static_cast<int>(res_result->status())});
  }

  prepared_ = true;
  co_return PgResult<void>{};
}

boost::asio::awaitable<PgResult<Result>> PreparedStatement::execute(
    const std::vector<std::string>& params) {
  if (!prepared_) {
    auto prepare_result = co_await prepare();
    if (!prepare_result) {
      co_return std::unexpected(prepare_result.error());
    }
  }

  // Convert parameters
  std::vector<const char*> values;
  values.reserve(params.size());
  for (const auto& param : params) {
    values.push_back(param.c_str());
  }

  if (!PQsendQueryPrepared(conn_.native_handle(), name_.c_str(), static_cast<int>(values.size()),
                           values.data(),
                           nullptr,  // param lengths - null-terminated strings
                           nullptr,  // param formats - text format
                           0         // result format - text format
                           )) {
    co_return std::unexpected(PgError::from_conn(conn_.native_handle()));
  }

  auto flush_result = co_await conn_.flush_outgoing_data();
  if (!flush_result) {
    co_return std::unexpected(flush_result.error());
  }

  co_return co_await conn_.get_query_result();
}

boost::asio::awaitable<PgResult<void>> PreparedStatement::deallocate() {
  if (!prepared_) {
    co_return PgResult<void>{};
  }

  const std::string deallocate_cmd = "DEALLOCATE " + name_;
  auto res_result = co_await conn_.query(deallocate_cmd);

  if (!res_result) {
    co_return std::unexpected(res_result.error());
  }

  if (!*res_result) {
    co_return std::unexpected(PgError{.message = res_result->error_message(),
                                      .error_code = static_cast<int>(res_result->status())});
  }

  prepared_ = false;
  co_return PgResult<void>{};
}

}  // namespace relx::pgsql_async_wrapper