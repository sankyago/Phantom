#pragma once

#include "client/entities/entity_manager.h"
#include "client/game/game_state.h"
#include "hook/fiber.h"

#include <functional>
#include <memory>

namespace phantom::client {

// ---------------------------------------------------------------------------
// ScriptThread
// ---------------------------------------------------------------------------

class ScriptThread {
public:
    ScriptThread(hook::FiberManager& fiber_manager,
                 GameState& game_state,
                 EntityManager& entity_manager);

    void start();

private:
    void script_main();

    hook::FiberManager& fiber_manager_;
    GameState& game_state_;
    EntityManager& entity_manager_;
    bool initialized_ = false;
};

} // namespace phantom::client
