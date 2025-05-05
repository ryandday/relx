#include "relx/connection/pgsql_async_wrapper.hpp"
#include <regex>

namespace relx::pgsql_async_wrapper {

// Helper function to convert ? placeholders to $1, $2, etc.
std::string convert_placeholders(const std::string& sql) {
    std::regex placeholder_regex("\\?");
    std::string result;
    std::string::const_iterator search_start(sql.cbegin());
    std::smatch match;
    int placeholder_count = 1;

    while (std::regex_search(search_start, sql.cend(), match, placeholder_regex)) {
        result.append(search_start, match[0].first);
        result.append("$" + std::to_string(placeholder_count++));
        search_start = match[0].second;
    }

    result.append(search_start, sql.cend());
    return result;
}

// Implementation of prepared_statement methods
boost::asio::awaitable<void> prepared_statement::prepare() {
    if (prepared_) {
        co_return;
    }
    
    // Convert ? placeholders to $n format
    std::string pg_query = convert_placeholders(query_);
    
    if (!PQsendPrepare(conn_.native_handle(), name_.c_str(), pg_query.c_str(), 0, nullptr)) {
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