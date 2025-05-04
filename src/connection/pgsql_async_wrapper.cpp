#include "relx/connection/pgsql_async_wrapper.hpp"

namespace relx::pgsql_async_wrapper {

// Implementation of prepared_statement methods
boost::asio::awaitable<void> prepared_statement::prepare() {
    if (prepared_) {
        co_return;
    }
    
    if (!PQsendPrepare(conn_.native_handle(), name_.c_str(), query_.c_str(), 0, nullptr)) {
        throw statement_error(conn_.native_handle());
    }
    
    co_await conn_.flush_outgoing_data();
    result res = co_await conn_.get_query_result();
    
    if (!res) {
        throw statement_error(res.error_message());
    }
    
    prepared_ = true;
    co_return;
}

boost::asio::awaitable<result> prepared_statement::execute(const std::vector<std::string>& params) {
    if (!prepared_) {
        co_await prepare();
    }
    
    // Convert parameters
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& param : params) {
        values.push_back(param.c_str());
    }
    
    if (!PQsendQueryPrepared(
            conn_.native_handle(),
            name_.c_str(),
            static_cast<int>(values.size()),
            values.data(),
            nullptr, // param lengths - null-terminated strings
            nullptr, // param formats - text format
            0 // result format - text format
        )) {
        throw statement_error(conn_.native_handle());
    }
    
    co_await conn_.flush_outgoing_data();
    co_return co_await conn_.get_query_result();
}

boost::asio::awaitable<void> prepared_statement::deallocate() {
    if (!prepared_) {
        co_return;
    }
    
    std::string deallocate_cmd = "DEALLOCATE " + name_;
    result res = co_await conn_.query(deallocate_cmd);
    
    if (!res) {
        throw statement_error(res.error_message());
    }
    
    prepared_ = false;
    co_return;
}

} // namespace relx::pgsql_async_wrapper 