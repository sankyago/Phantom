#pragma once
#include <cstdint>
#include <string_view>

namespace phantom
{
inline constexpr std::string_view PHANTOM_VERSION = "0.1.0";
inline constexpr uint16_t DEFAULT_SERVER_PORT = 7788;
inline constexpr uint32_t TICK_RATE_HZ = 30;
inline constexpr uint32_t PROTOCOL_VERSION = 1;
inline constexpr uint32_t MAX_PLAYERS = 32;
inline constexpr uint32_t RECONNECT_DELAY_MS = 3000;
inline constexpr uint32_t MAX_RECONNECT_ATTEMPTS = 5;
} // namespace phantom
