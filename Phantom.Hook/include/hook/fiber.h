#pragma once

#include "hook/vmt_hook.h"

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <vector>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// ScriptCallback
// ---------------------------------------------------------------------------

using ScriptCallback = std::function<void()>;

// ---------------------------------------------------------------------------
// FiberManager
// ---------------------------------------------------------------------------

class FiberManager {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<FiberManager>, HookError>
    create();

    ~FiberManager();

    FiberManager(const FiberManager&) = delete;
    FiberManager& operator=(const FiberManager&) = delete;

    void add_script(ScriptCallback callback);
    void tick();
    static void yield();

private:
    FiberManager() = default;

    struct ScriptFiber {
        LPVOID fiber = nullptr;
        ScriptCallback callback;
        bool finished = false;
    };

    static void CALLBACK fiber_entry(LPVOID param);

    LPVOID main_fiber_ = nullptr;
    std::vector<ScriptFiber> scripts_;

    static std::atomic<FiberManager*> instance_;
};

} // namespace phantom::hook
