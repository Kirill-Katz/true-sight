#include <boost/asio/awaitable.hpp>
#include <boost/beast/websocket/impl/rfc6455.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "controllers/conversations.hpp"
#include "controllers/healthcheck.hpp"
#include "controllers/get_challenge.hpp"
#include "controllers/validate_challenge.hpp"
#include "controllers/messages.hpp"
#include "controllers/register.hpp"

#include "db/database.hpp"
#include "http/router.hpp"
#include "middleware/auth_middleware.hpp"
#include "shared.hpp"
#include "ws/ws.hpp"

std::string get_required_environment_variable(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error(
            std::string("Required environment variable is missing: ") + name
        );
    }

    return value;
}

std::size_t get_port() {
    const std::string port = get_required_environment_variable("PORT");
    std::size_t parsed_length = 0;
    const unsigned long parsed_port = std::stoul(port, &parsed_length);

    if (parsed_length != port.size() || parsed_port == 0 || parsed_port > 65535) {
        throw std::runtime_error("PORT must be an integer between 1 and 65535");
    }

    return parsed_port;
}


asio::awaitable<void> handle_ws_upgrade(
    tcp::socket socket,
    http::request<http::string_body> request,
    WebSocket& ws
) {
    auto ctx = RequestContext{
        .request = request
    };

    AuthMiddleware auth_check;
    auto res = co_await auth_check.handle(ctx);

    if (res.has_value()) {
        beast::error_code ec;

        co_await beast::async_write(
            socket,
            std::move(*res),
            asio::redirect_error(asio::use_awaitable, ec)
        );

        co_return;
    }

    co_await ws.accept(
        *ctx.authenticated_iid,
        std::move(socket),
        std::move(request)
    );
}

asio::awaitable<void> session(
    tcp::socket socket,
    Router& router,
    WebSocket& ws
) {
    beast::flat_buffer buffer;

    for (;;) {
        http::request<http::string_body> request;
        beast::error_code ec;
        co_await http::async_read(
            socket,
            buffer,
            request,
            asio::redirect_error(asio::use_awaitable, ec)
        );

        if (ec == http::error::end_of_stream) {
            break;
        }

        if (ec) {
            co_return;
        }

        if (
            request.target() == "/ws" &&
            websocket::is_upgrade(request)
        ) {
            co_await handle_ws_upgrade(
                std::move(socket),
                std::move(request),
                ws
            );

            co_return;
        }

        auto response = co_await router.route(request);
        bool keep_alive = response.keep_alive();

        co_await beast::async_write(
            socket,
            std::move(response),
            asio::redirect_error(asio::use_awaitable, ec)
        );

        if (ec) {
            co_return;
        }

        if (!keep_alive) {
            break;
        }
    }

    beast::error_code ec;
    auto _ = socket.shutdown(tcp::socket::shutdown_send, ec);
}

asio::awaitable<void> listen(
    asio::io_context& io,
    Router& router,
    WebSocket& ws,
    std::size_t port
) {
    tcp::acceptor acceptor(
        io,
        tcp::endpoint(tcp::v4(), port)
    );

    std::cout << "Listening on port " << port << '\n';
    for (;;) {
        tcp::socket socket =
            co_await acceptor.async_accept(asio::use_awaitable);

        asio::co_spawn(
            io,
            session(std::move(socket), router, ws),
            asio::detached
        );
    }
}

int main() {
    asio::io_context io;
    Database db{
        get_required_environment_variable("DATABASE_URL"),
        4
    };

    WebSocket ws;

    std::vector<std::shared_ptr<Middleware>> auth_check;
    auth_check.push_back(std::make_shared<AuthMiddleware>());

    Router router{};
    router.register_path("/health-check", std::make_unique<HealthCheckController>(db, ws));
    router.register_path("/get-challenge", std::make_unique<GetChallengeController>(db));
    router.register_path("/validate-challenge", std::make_unique<ValidateChallengeController>(db));
    router.register_path("/register", std::make_unique<RegisterController>(db));
    router.register_path(
        "/messages",
        std::make_unique<MessagesController>(db, ws),
        auth_check
    );

    router.register_path(
        "/conversations",
        std::make_unique<ConversationsController>(db),
        auth_check
    );

    asio::co_spawn(
        io,
        listen(io, router, ws, get_port()),
        asio::detached
    );

    io.run();
}
