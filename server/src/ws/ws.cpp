#include <boost/asio/awaitable.hpp>

#include "ws/ws.hpp"

asio::awaitable<void> WebSocket::accept(
    std::string user_iid,
    tcp::socket socket,
    http::request<http::string_body> request
) {
    auto session = std::make_shared<WebSocketSession>(
        std::move(socket)
    );

    bool success = co_await session->accept(
        std::move(request)
    );

    if (!success) {
        co_return;
    }

    sessions_.insert_or_assign(
        user_iid,
        session
    );

    co_await session->run();

    auto it = sessions_.find(user_iid);
    if (
        it != sessions_.end() &&
        it->second == session
    ) {
        sessions_.erase(it);
    }
}

void WebSocket::notify(
    std::string user_iid,
    const boost::json::object& message
) {
    auto it = sessions_.find(user_iid);

    if (it == sessions_.end()) {
        return;
    }

    it->second->notify(
        std::move(message)
    );
}

void WebSocket::notify_all(
    const boost::json::object& message
) {
    for (auto& [user, session] : sessions_) {
        session->notify(
            message
        );
    }
}
