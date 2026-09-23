#include "ws/ws_session.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>

#include <boost/json.hpp>
#include <iostream>

asio::awaitable<bool> WebSocketSession::accept(
    http::request<http::string_body> request
) {
    beast::error_code ec;
    co_await ws_.async_accept(
        request,
        asio::redirect_error(
            asio::use_awaitable,
            ec
        )
    );

    if (ec) {
        co_return false;
    }

    co_return true;
}

asio::awaitable<void> WebSocketSession::run() {
    for (;;) {
        beast::error_code ec;

        co_await ws_.async_read(
            buffer_,
            asio::redirect_error(
                asio::use_awaitable,
                ec
            )
        );

        if (ec == websocket::error::closed) {
            co_return;
        }

        if (ec) {
            co_return;
        }

        auto message = beast::buffers_to_string(buffer_.data());

        buffer_.consume(buffer_.size());

        std::cout << "WS: " << message << '\n';
    }
};

void WebSocketSession::notify(
    boost::json::object message
) {
    asio::post(
        strand_,
        [
            self = shared_from_this(),
            message = std::move(message)
        ] () mutable {
            self->outgoing_.push_back(
                boost::json::serialize(message)
            );

            if (self->writing_) {
                return;
            }

            self->writing_ = true;

            asio::co_spawn(
                self->strand_,
                self->write_loop(),
                asio::detached
            );
        }
    );
}

asio::awaitable<void> WebSocketSession::write_loop() {
    while (!outgoing_.empty()) {
        beast::error_code ec;

        co_await ws_.async_write(
            asio::buffer(outgoing_.front()),
            asio::redirect_error(
                asio::use_awaitable,
                ec
            )
        );

        if (ec) {
            writing_ = false;
            co_return;
        }

        outgoing_.pop_front();
    }

    writing_ = false;
}
