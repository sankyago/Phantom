#include "client/game/game_state.h"

namespace phantom::client {

// ---------------------------------------------------------------------------
// game_error_to_string
// ---------------------------------------------------------------------------

const char* game_error_to_string(GameError error) {
    switch (error) {
    case GameError::NativeCallFailed: return "NativeCallFailed";
    case GameError::NotInitialized:   return "NotInitialized";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// GameState
// ---------------------------------------------------------------------------

GameState::GameState(const hook::NativeInvoker& invoker)
    : invoker_(invoker) {}

std::expected<void, GameError> GameState::disable_singleplayer_flow() {
    // SET_THIS_SCRIPT_CAN_BE_PAUSED(false)
    auto r1 = invoker_.invoke_void(0xAA391C728106F7AF, false);
    if (!r1.has_value())
        return std::unexpected(GameError::NativeCallFailed);

    // DESTROY_ALL_CAMS(false)
    auto r2 = invoker_.invoke_void(0x8999F5B72C283323, false);
    if (!r2.has_value())
        return std::unexpected(GameError::NativeCallFailed);

    return {};
}

std::expected<void, GameError> GameState::cleanup_world() {
    // CLEAR_AREA(0, 0, 0, 10000, true, false, false, false)
    auto result = invoker_.invoke_void(
        0xA56F01F3765B93A0,
        0.0f, 0.0f, 0.0f,   // position (origin)
        10000.0f,            // radius
        true, false, false, false);

    if (!result.has_value())
        return std::unexpected(GameError::NativeCallFailed);

    return {};
}

std::expected<void, GameError> GameState::setup_multiplayer_environment() {
    auto r1 = disable_singleplayer_flow();
    if (!r1.has_value())
        return r1;

    auto r2 = cleanup_world();
    if (!r2.has_value())
        return r2;

    return {};
}

} // namespace phantom::client
