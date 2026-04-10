#include "hook/fiber.h"

namespace phantom::hook {

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------

FiberManager* FiberManager::instance_ = nullptr;

// ---------------------------------------------------------------------------
// FiberManager::create
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<FiberManager>, HookError> FiberManager::create() {
    auto self = std::unique_ptr<FiberManager>(new FiberManager());

    // If the current thread is already a fiber, use it; otherwise convert.
    self->main_fiber_ = ::GetCurrentFiber();
    if (!self->main_fiber_ || self->main_fiber_ == reinterpret_cast<LPVOID>(0x1E00)) {
        // 0x1E00 is the sentinel value returned when the thread is not a fiber.
        self->main_fiber_ = ::ConvertThreadToFiber(nullptr);
    }

    if (!self->main_fiber_) {
        return std::unexpected(HookError::NullPointer);
    }

    instance_ = self.get();
    return self;
}

// ---------------------------------------------------------------------------
// FiberManager::add_script
// ---------------------------------------------------------------------------

void FiberManager::add_script(ScriptCallback callback) {
    scripts_.push_back(ScriptFiber{
        .fiber = nullptr,
        .callback = std::move(callback),
        .finished = false,
    });

    auto& sf = scripts_.back();
    sf.fiber = ::CreateFiber(0, &fiber_entry, &sf);
}

// ---------------------------------------------------------------------------
// FiberManager::tick
// ---------------------------------------------------------------------------

void FiberManager::tick() {
    for (auto& sf : scripts_) {
        if (!sf.finished && sf.fiber) {
            ::SwitchToFiber(sf.fiber);
        }
    }
}

// ---------------------------------------------------------------------------
// FiberManager::yield
// ---------------------------------------------------------------------------

void FiberManager::yield() {
    if (instance_ && instance_->main_fiber_) {
        ::SwitchToFiber(instance_->main_fiber_);
    }
}

// ---------------------------------------------------------------------------
// FiberManager::fiber_entry
// ---------------------------------------------------------------------------

void CALLBACK FiberManager::fiber_entry(LPVOID param) {
    auto* sf = static_cast<ScriptFiber*>(param);

    if (sf->callback) {
        sf->callback();
    }

    sf->finished = true;

    // Switch back to the main fiber — this fiber is done.
    if (instance_ && instance_->main_fiber_) {
        ::SwitchToFiber(instance_->main_fiber_);
    }
}

} // namespace phantom::hook
