#pragma once

#include "hook/natives.h"

#include <expected>

namespace phantom::client {

// ---------------------------------------------------------------------------
// GameError
// ---------------------------------------------------------------------------

enum class GameError { NativeCallFailed, NotInitialized };

[[nodiscard]] const char* game_error_to_string(GameError error);

// ---------------------------------------------------------------------------
// GameState
// ---------------------------------------------------------------------------

class GameState {
public:
    explicit GameState(const hook::NativeInvoker& invoker);

    [[nodiscard]] std::expected<void, GameError> disable_singleplayer_flow();
    [[nodiscard]] std::expected<void, GameError> cleanup_world();
    [[nodiscard]] std::expected<void, GameError> setup_multiplayer_environment();

private:
    const hook::NativeInvoker& invoker_;
};

} // namespace phantom::client
