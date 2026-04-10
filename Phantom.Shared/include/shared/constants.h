#pragma once
#include <cstdint>

namespace phantom
{
  constexpr const char* PHANTOM_VERSION = "0.1.0";
  constexpr uint16_t DEFAULT_SERVER_PORT = 7788;
  constexpr uint32_t TICK_RATE_HZ = 30;
  constexpr uint32_t PROTOCOL_VERSION = 1;
  constexpr uint32_t MAX_PLAYERS = 32;
  constexpr uint32_t RECONNECT_DELAY_MS = 3000;
  constexpr uint32_t MAX_RECONNECT_ATTEMPTS = 5;
}
