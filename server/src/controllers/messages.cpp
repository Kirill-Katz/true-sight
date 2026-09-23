#include "controllers/messages.hpp"
#include "crypto/crypto.hpp"
#include "shared.hpp"
#include "http/default_responses.hpp"
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/status.hpp>
#include <span>
#include <unordered_map>

std::basic_string<std::byte> to_pqxx_bytes(std::span<std::byte> bytes) {
    return {bytes.begin(), bytes.end()};
}


// /messages?from=Bob&limit=50&offset=150

AsyncResponse MessagesController::GET(RequestContext& ctx) {
    auto to = ctx.authenticated_iid;
    if (!to.has_value()) {
        co_return responses::unauthorized(ctx, "The JWT token does not contain the user iid");
    }

    std::unordered_map<std::string, std::string> params;
    for (const auto& param : ctx.url.params()) {
        params[param.key] = param.value;
    }

    if (!params.contains("from")) {
        co_return responses::bad_request(ctx, "from query param is required");
    }

    auto from = params["from"];
    int64_t limit = params.contains("limit")
        ? static_cast<int64_t>(std::stoll(params["limit"]))
        : 50;

    int64_t offset = params.contains("offset")
        ? static_cast<int64_t>(std::stoll(params["offset"]))
        : 0;

    auto conversation_res = co_await db_.exec_async(
        "SELECT * FROM ("
        "SELECT * FROM messages WHERE "
        "((sender_iid = $1 AND receiver_iid = $2) OR "
        "(receiver_iid = $1 AND sender_iid = $2)) "
        "ORDER BY created_at DESC, message_id DESC "
        "LIMIT $3 OFFSET $4"
        ") AS recent_messages "
        "ORDER BY created_at ASC, message_id ASC;",
        pqxx::params{from, *to, limit, offset},
        asio::use_awaitable
    );

    boost::json::array messages;

    for (const auto& message : conversation_res) {
        auto ciphertext = message["ciphertext"].as<pqxx::binarystring>();
        auto auth_tag = message["auth_tag"].as<pqxx::binarystring>();
        auto nonce = message["nonce"].as<pqxx::binarystring>();

        std::string ciphertext_bytes {
            reinterpret_cast<const char*>(ciphertext.data()),
            ciphertext.size()
        };

        std::string auth_tag_bytes {
            reinterpret_cast<const char*>(auth_tag.data()),
            auth_tag.size()
        };

        std::string nonce_bytes {
            reinterpret_cast<const char*>(nonce.data()),
            nonce.size()
        };

        std::string signature_bytes;
        if (!message["signature"].is_null()) {
            auto signature = message["signature"].as<pqxx::binarystring>();
            signature_bytes.assign(
                reinterpret_cast<const char*>(signature.data()), signature.size()
            );
        }

        messages.push_back({
            {"sender_iid", message["sender_iid"].c_str()},
            {"receiver_iid", message["receiver_iid"].c_str()},
            {"ciphertext", crypto::base64url_encode(ciphertext_bytes)},
            {"auth_tag", crypto::base64url_encode(auth_tag_bytes)},
            {"nonce", crypto::base64url_encode(nonce_bytes)},
            {"signature", crypto::base64url_encode(signature_bytes)},
            {"created_at", message["created_at"].c_str()},
            {"protocol_version", message["protocol_version"].as<int64_t>()},
            {"message_counter", message["message_counter"].as<int64_t>()}
        });
    }

    co_return responses::json(
        ctx,
        http::status::ok,
        {
            {"status", "ok"},
            {"messages", std::move(messages)}
        }
    );
}

// {
//      "to": "Alice",
//      "nonce": <base64encoded>,
//      "ciphertext": <base64encoded>,
//      "auth_tag": <base64encoded>,
//      "protocol_version": 1,
//      "message_counter": 2
// }

constexpr std::array message_create{
    Field{"to", boost::json::kind::string},
    Field{"nonce", boost::json::kind::string},
    Field{"auth_tag", boost::json::kind::string},
    Field{"ciphertext", boost::json::kind::string},
    Field{"signature", boost::json::kind::string},
    Field{"protocol_version", boost::json::kind::int64},
    Field{"message_counter", boost::json::kind::int64}
};

void MessagesController::message_created(
    RequestContext& ctx,
    std::string message_id,
    std::string sender_iid,
    std::string receiver_iid
) {
    boost::json::object notification = {
        {"event", "message_created"},
        {"message_id", message_id},
        {"sender_iid", sender_iid},
        {"receiver_iid", receiver_iid}
    };

    ws_.notify(receiver_iid, notification);
}

AsyncResponse MessagesController::POST(RequestContext& ctx) {
    boost::json::value body;

    try {
        body = boost::json::parse(ctx.request.body());
    } catch (const boost::system::system_error&) {
        co_return responses::bad_request(ctx, "Invalid JSON");
    }

    if (!body.is_object()) {
        co_return responses::bad_request(ctx, "The requst must be a JSON object");
    }

    if (!ctx.authenticated_iid.has_value()) {
        co_return responses::unauthorized(ctx, "The JWT token does not contain the user iid");
    }

    boost::json::object& obj = body.as_object();

    auto invalid_fields_res = responses::fields_required(
        ctx,
        obj,
        message_create
    );

    if (invalid_fields_res.has_value()) {
        co_return std::move(*invalid_fields_res);
    }

    auto from = ctx.authenticated_iid.value();
    auto to = std::string(obj["to"].as_string());
    auto protocol_version = obj["protocol_version"].as_int64();
    auto message_counter = obj["message_counter"].as_int64();

    auto nonce = crypto::base64url_decode(obj["nonce"].as_string());
    auto ciphertext = crypto::base64url_decode(obj["ciphertext"].as_string());
    auto auth_tag = crypto::base64url_decode(obj["auth_tag"].as_string());
    auto signature = crypto::base64url_decode(obj["signature"].as_string());

    if (nonce.size() != 12) {
        co_return responses::bad_request(ctx, "Ivalid nonce");
    }

    if (auth_tag.size() != 16) {
        co_return responses::bad_request(ctx, "Ivalid auth tag");
    }

    if (ciphertext.empty()) {
        co_return responses::bad_request(ctx, "Invalid ciphertext");
    }

    if (signature.size() != 64) {
        co_return responses::bad_request(ctx, "Invalid signature");
    }

    if (protocol_version != 2) {
        co_return responses::bad_request(ctx, "Unsupported protocol version");
    }

    if (message_counter <= 0) {
        co_return responses::bad_request(ctx, "Invalid message counter");
    }

    auto nonce_bytes = std::span<std::byte>{
        reinterpret_cast<std::byte*>(nonce.data()),
        nonce.size()
    };

    auto ciphertext_bytes = std::span<std::byte>{
        reinterpret_cast<std::byte*>(ciphertext.data()),
        ciphertext.size()
    };

    auto auth_tag_bytes = std::span<std::byte>{
        reinterpret_cast<std::byte*>(auth_tag.data()),
        auth_tag.size()
    };

    auto signature_bytes = std::span<std::byte>{
        reinterpret_cast<std::byte*>(signature.data()),
        signature.size()
    };

    auto res = co_await db_.exec_async(
        "INSERT INTO messages "
        "(sender_iid, receiver_iid, nonce, ciphertext, auth_tag, signature, protocol_version, message_counter) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
        "RETURNING message_id;",
        pqxx::params{
            from,
            to,
            to_pqxx_bytes(nonce_bytes),
            to_pqxx_bytes(ciphertext_bytes),
            to_pqxx_bytes(auth_tag_bytes),
            to_pqxx_bytes(signature_bytes),
            protocol_version,
            message_counter
        },
        asio::use_awaitable
    );

    auto message_id = res[0]["message_id"].as<std::string>();
    message_created(ctx, std::move(message_id), from, to);

    co_return responses::json(ctx, http::status::ok, {{ "status", "ok" }});
}
