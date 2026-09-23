#pragma once

#include "ws/ws_session.hpp"

#include <boost/asio/awaitable.hpp>
#include <unordered_map>

class WebSocket {
public:
    asio::awaitable<void> accept(
        std::string user_iid,
        tcp::socket socket,
        http::request<http::string_body> request
    );

    void notify(
        std::string user_iid,
        const boost::json::object& message
    );

    void notify_all(
        const boost::json::object& message
    );

private:
    std::unordered_map<
        std::string,
        std::shared_ptr<WebSocketSession>
    > sessions_;
};
