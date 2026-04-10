#include "client/game/script_thread.h"

namespace phantom::client {

ScriptThread::ScriptThread(hook::FiberManager& fiber_manager,
                           GameState& game_state,
                           EntityManager& entity_manager)
    : fiber_manager_(fiber_manager)
    , game_state_(game_state)
    , entity_manager_(entity_manager) {}

void ScriptThread::start() {
    fiber_manager_.add_script([this]() { script_main(); });
}

void ScriptThread::script_main() {
    while (true) {
        if (!initialized_) {
            auto result = game_state_.setup_multiplayer_environment();
            if (result.has_value())
                initialized_ = true;
        }

        hook::FiberManager::yield();
    }
}

} // namespace phantom::client
