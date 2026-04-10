#pragma once

#include "hook/vmt_hook.h"

#include <expected>
#include <functional>
#include <memory>
#include <vector>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// InputCallback — return true if the message was consumed.
// ---------------------------------------------------------------------------

using InputCallback = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

// ---------------------------------------------------------------------------
// InputHook
// ---------------------------------------------------------------------------

class InputHook {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<InputHook>, HookError>
    create(HWND hwnd);

    ~InputHook();

    InputHook(const InputHook&) = delete;
    InputHook& operator=(const InputHook&) = delete;

    void add_callback(InputCallback callback);

private:
    explicit InputHook(HWND hwnd, WNDPROC original);

    static LRESULT CALLBACK hooked_wndproc(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HWND hwnd_ = nullptr;
    WNDPROC original_wndproc_ = nullptr;
    std::vector<InputCallback> callbacks_;

    static InputHook* instance_;
};

} // namespace phantom::hook
