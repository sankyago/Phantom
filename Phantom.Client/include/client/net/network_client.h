#pragma once
#include "shared/constants.h"
#include "shared/entity_types.h"
#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace phantom::client
{
enum class NetworkError { ConnectionFailed, Disconnected, SendFailed, InvalidMessage };
[[nodiscard]] const char* network_error_to_string(NetworkError error);

struct PlayerConnectedEvent { uint32_t player_id; std::string name; };
struct PlayerDisconnectedEvent { uint32_t player_id; };
struct EntityUpdateEvent { uint32_t entity_id; EntityType entity_type; uint32_t model_hash; Vec3 position; Vec3 rotation; };

using NetworkEvent = std::variant<PlayerConnectedEvent, PlayerDisconnectedEvent, EntityUpdateEvent>;

class NetworkClient
{
  public:
    NetworkClient();
    ~NetworkClient();
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    [[nodiscard]] std::expected<void, NetworkError> connect(const std::string& url);
    void disconnect();
    [[nodiscard]] bool is_connected() const noexcept { return connected_.load(); }

    [[nodiscard]] std::expected<void, NetworkError> send_entity_update(uint32_t entity_id, EntityType type, uint32_t model_hash, const Vec3& position, const Vec3& rotation);
    [[nodiscard]] std::expected<void, NetworkError> send_chat_message(const std::string& text);
    [[nodiscard]] std::vector<NetworkEvent> poll_events();

  private:
    void network_thread_func(const std::string& url);
    void enqueue_event(NetworkEvent event);

    std::atomic<bool> connected_{false};
    std::atomic<bool> should_stop_{false};
    std::thread network_thread_;
    std::mutex event_mutex_;
    std::queue<NetworkEvent> event_queue_;
    std::mutex send_mutex_;
    void* ws_handle_ = nullptr;
};
} // namespace phantom::client
