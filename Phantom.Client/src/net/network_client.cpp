#include "client/net/network_client.h"
#include <ixwebsocket/IXWebSocket.h>
#include <chrono>

namespace phantom::client
{
const char* network_error_to_string(NetworkError error)
{
    switch (error)
    {
        case NetworkError::ConnectionFailed: return "ConnectionFailed";
        case NetworkError::Disconnected:     return "Disconnected";
        case NetworkError::SendFailed:       return "SendFailed";
        case NetworkError::InvalidMessage:   return "InvalidMessage";
    }
    return "Unknown";
}

NetworkClient::NetworkClient() = default;

NetworkClient::~NetworkClient()
{
    disconnect();
}

std::expected<void, NetworkError> NetworkClient::connect(const std::string& url)
{
    if (connected_.load())
        disconnect();

    should_stop_.store(false);
    network_thread_ = std::thread(&NetworkClient::network_thread_func, this, url);

    // Wait briefly for connection to establish
    for (int i = 0; i < 50 && !connected_.load() && !should_stop_.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!connected_.load())
        return std::unexpected(NetworkError::ConnectionFailed);

    return {};
}

void NetworkClient::disconnect()
{
    should_stop_.store(true);

    if (network_thread_.joinable())
        network_thread_.join();

    connected_.store(false);
}

void NetworkClient::network_thread_func(const std::string& url)
{
    ix::WebSocket ws;
    ws.setUrl(url);

    {
        std::lock_guard lock(send_mutex_);
        ws_handle_ = &ws;
    }

    ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type)
        {
            case ix::WebSocketMessageType::Open:
                connected_.store(true);
                break;

            case ix::WebSocketMessageType::Close:
                connected_.store(false);
                break;

            case ix::WebSocketMessageType::Error:
                connected_.store(false);
                break;

            case ix::WebSocketMessageType::Message:
                // TODO: FlatBuffers deserialization
                // Parse msg->str and enqueue appropriate NetworkEvent
                break;

            default:
                break;
        }
    });

    ws.start();

    uint32_t reconnect_attempts = 0;

    while (!should_stop_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!connected_.load() && reconnect_attempts < MAX_RECONNECT_ATTEMPTS)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
            if (should_stop_.load())
                break;

            ws.stop();
            ws.start();
            ++reconnect_attempts;
        }
        else if (connected_.load())
        {
            reconnect_attempts = 0;
        }
    }

    ws.stop();

    {
        std::lock_guard lock(send_mutex_);
        ws_handle_ = nullptr;
    }

    connected_.store(false);
}

std::expected<void, NetworkError> NetworkClient::send_entity_update(
    [[maybe_unused]] uint32_t entity_id,
    [[maybe_unused]] EntityType type,
    [[maybe_unused]] uint32_t model_hash,
    [[maybe_unused]] const Vec3& position,
    [[maybe_unused]] const Vec3& rotation)
{
    if (!connected_.load())
        return std::unexpected(NetworkError::Disconnected);

    std::lock_guard lock(send_mutex_);
    auto* ws = static_cast<ix::WebSocket*>(ws_handle_);
    if (!ws)
        return std::unexpected(NetworkError::Disconnected);

    // TODO: FlatBuffers serialization
    // Build entity update message and send via ws->sendBinary(...)

    return {};
}

std::expected<void, NetworkError> NetworkClient::send_chat_message(
    [[maybe_unused]] const std::string& text)
{
    if (!connected_.load())
        return std::unexpected(NetworkError::Disconnected);

    std::lock_guard lock(send_mutex_);
    auto* ws = static_cast<ix::WebSocket*>(ws_handle_);
    if (!ws)
        return std::unexpected(NetworkError::Disconnected);

    // TODO: FlatBuffers serialization
    // Build chat message and send via ws->sendBinary(...)

    return {};
}

std::vector<NetworkEvent> NetworkClient::poll_events()
{
    std::vector<NetworkEvent> events;
    std::lock_guard lock(event_mutex_);

    while (!event_queue_.empty())
    {
        events.push_back(std::move(event_queue_.front()));
        event_queue_.pop();
    }

    return events;
}

void NetworkClient::enqueue_event(NetworkEvent event)
{
    std::lock_guard lock(event_mutex_);
    event_queue_.push(std::move(event));
}
} // namespace phantom::client
