#include "client/net/network_client.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winhttp.h>
#include <chrono>
#include <array>

#pragma comment(lib, "winhttp.lib")

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

void NetworkClient::network_thread_func(const std::string& /*url*/)
{
    // Open a WinHTTP session
    HINTERNET session = ::WinHttpOpen(L"Phantom/1.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return;

    // Connect to localhost:7788
    HINTERNET connection = ::WinHttpConnect(session, L"localhost", 7788, 0);
    if (!connection)
    {
        ::WinHttpCloseHandle(session);
        return;
    }

    // Open a request to upgrade to WebSocket
    HINTERNET request = ::WinHttpOpenRequest(connection, L"GET", L"/",
                                              nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!request)
    {
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return;
    }

    // Set WebSocket upgrade option
    if (!::WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
    {
        ::WinHttpCloseHandle(request);
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return;
    }

    // Send the request
    if (!::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    {
        ::WinHttpCloseHandle(request);
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return;
    }

    if (!::WinHttpReceiveResponse(request, nullptr))
    {
        ::WinHttpCloseHandle(request);
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return;
    }

    // Complete the WebSocket upgrade
    HINTERNET websocket = ::WinHttpWebSocketCompleteUpgrade(request, 0);
    ::WinHttpCloseHandle(request);

    if (!websocket)
    {
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return;
    }

    {
        std::lock_guard lock(send_mutex_);
        ws_handle_ = websocket;
    }

    connected_.store(true);

    // Receive loop
    std::array<uint8_t, 4096> buffer{};

    while (!should_stop_.load())
    {
        DWORD bytes_read = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE buffer_type{};

        DWORD error = ::WinHttpWebSocketReceive(websocket,
                                                 buffer.data(),
                                                 static_cast<DWORD>(buffer.size()),
                                                 &bytes_read,
                                                 &buffer_type);

        if (error != ERROR_SUCCESS)
        {
            connected_.store(false);
            break;
        }

        if (buffer_type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
        {
            connected_.store(false);
            break;
        }

        // TODO: FlatBuffers deserialization of buffer[0..bytes_read]
    }

    {
        std::lock_guard lock(send_mutex_);
        ws_handle_ = nullptr;
    }

    ::WinHttpWebSocketClose(websocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    ::WinHttpCloseHandle(websocket);
    ::WinHttpCloseHandle(connection);
    ::WinHttpCloseHandle(session);

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
    auto* ws = static_cast<HINTERNET>(ws_handle_);
    if (!ws)
        return std::unexpected(NetworkError::Disconnected);

    // TODO: FlatBuffers serialization
    // Build entity update message and send via WinHttpWebSocketSend

    return {};
}

std::expected<void, NetworkError> NetworkClient::send_chat_message(
    [[maybe_unused]] const std::string& text)
{
    if (!connected_.load())
        return std::unexpected(NetworkError::Disconnected);

    std::lock_guard lock(send_mutex_);
    auto* ws = static_cast<HINTERNET>(ws_handle_);
    if (!ws)
        return std::unexpected(NetworkError::Disconnected);

    // TODO: FlatBuffers serialization
    // Build chat message and send via WinHttpWebSocketSend

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
