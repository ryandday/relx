#pragma once

#include "http.hpp"
#include "router.hpp"

#include <chrono>
#include <exception>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>

namespace relx::web {

namespace beast = boost::beast;
using tcp = asio::ip::tcp;

/// @brief One HTTP connection: read requests, dispatch through the router,
/// write responses, until the client closes or keep-alive ends.
inline asio::awaitable<void> session(beast::tcp_stream stream, const Router& router) {
  beast::flat_buffer buffer;
  for (;;) {
    stream.expires_after(std::chrono::seconds(30));

    Request req;
    auto [read_ec, read_bytes] =
        co_await http::async_read(stream, buffer, req, asio::as_tuple(asio::use_awaitable));
    if (read_ec) {
      break;  // client closed, timeout, or malformed request
    }

    Response res;
    try {
      res = co_await router.dispatch(req);
    } catch (const std::exception& e) {
      res = error_response(http::status::internal_server_error, e.what());
    }
    res.keep_alive(req.keep_alive());
    res.prepare_payload();

    auto [write_ec, write_bytes] =
        co_await http::async_write(stream, res, asio::as_tuple(asio::use_awaitable));
    if (write_ec || !res.keep_alive()) {
      break;
    }
  }
  beast::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}

/// @brief Accept loop: spawn a session per connection
inline asio::awaitable<void> listener(tcp::endpoint endpoint, const Router& router) {
  auto executor = co_await asio::this_coro::executor;
  tcp::acceptor acceptor(executor, endpoint);
  for (;;) {
    auto [ec, socket] = co_await acceptor.async_accept(asio::as_tuple(asio::use_awaitable));
    if (ec) {
      continue;
    }
    asio::co_spawn(executor, session(beast::tcp_stream(std::move(socket)), router),
                   asio::detached);
  }
}

/// @brief Spawn the accept loop for `router` on `io`
inline void spawn_listener(asio::io_context& io, unsigned short port, const Router& router) {
  asio::co_spawn(io, listener(tcp::endpoint(tcp::v4(), port), router), asio::detached);
}

}  // namespace relx::web
