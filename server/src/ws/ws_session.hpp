#pragma once

#include "shared.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>
#include <deque>

class WebSocketSession
  : public std::enable_shared_from_this<WebSocketSession> {

public:
    explicit WebSocketSession(tcp::socket socket)
        : ws_(std::move(socket)),
          strand_(asio::make_strand(ws_.get_executor()))
    {}

    asio::awaitable<void> run();

    asio::awaitable<bool> accept(
        http::request<http::string_body> request
    );

    void notify(
        boost::json::object message
    );

private:
    asio::awaitable<void> write_loop();

    websocket::stream<tcp::socket> ws_;
    asio::strand<asio::any_io_executor> strand_;

    beast::flat_buffer buffer_;
    std::deque<std::string> outgoing_;
    bool writing_ = false;
};
