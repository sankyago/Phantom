#include "hook/input.h"

namespace phantom::hook {

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------

InputHook* InputHook::instance_ = nullptr;

// ---------------------------------------------------------------------------
// InputHook
// ---------------------------------------------------------------------------

InputHook::InputHook(HWND hwnd, WNDPROC original)
    : hwnd_(hwnd), original_wndproc_(original) {}

InputHook::~InputHook() {
    if (hwnd_ && original_wndproc_) {
        ::SetWindowLongPtrW(
            hwnd_, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(original_wndproc_));
    }
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// InputHook::create
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<InputHook>, HookError>
InputHook::create(HWND hwnd) {
    if (!hwnd || !::IsWindow(hwnd)) {
        return std::unexpected(HookError::NullPointer);
    }

    WNDPROC original = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&hooked_wndproc)));

    if (!original) {
        return std::unexpected(HookError::ProtectionChangeFailed);
    }

    auto self = std::unique_ptr<InputHook>(new InputHook(hwnd, original));
    instance_ = self.get();

    return self;
}

// ---------------------------------------------------------------------------
// InputHook::add_callback
// ---------------------------------------------------------------------------

void InputHook::add_callback(InputCallback callback) {
    callbacks_.push_back(std::move(callback));
}

// ---------------------------------------------------------------------------
// InputHook::hooked_wndproc
// ---------------------------------------------------------------------------

LRESULT CALLBACK InputHook::hooked_wndproc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    if (instance_) {
        for (auto& cb : instance_->callbacks_) {
            if (cb(hwnd, msg, wparam, lparam)) {
                return TRUE;
            }
        }
        return ::CallWindowProcW(
            instance_->original_wndproc_, hwnd, msg, wparam, lparam);
    }

    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace phantom::hook
