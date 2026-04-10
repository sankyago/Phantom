#pragma once

#include "hook/vmt_hook.h"

#include <concepts>
#include <expected>
#include <utility>

#include <detours.h>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// DetoursHook
// ---------------------------------------------------------------------------

template <typename Func>
    requires std::is_function_v<std::remove_pointer_t<Func>>
class DetoursHook {
public:
    [[nodiscard]] static std::expected<DetoursHook, HookError> create(Func target, Func detour) {
        if (!target || !detour) {
            return std::unexpected(HookError::NullPointer);
        }

        Func original = target;

        if (::DetourTransactionBegin() != NO_ERROR) {
            return std::unexpected(HookError::DetoursFailed);
        }

        if (::DetourUpdateThread(::GetCurrentThread()) != NO_ERROR) {
            ::DetourTransactionAbort();
            return std::unexpected(HookError::DetoursFailed);
        }

        if (::DetourAttach(reinterpret_cast<PVOID*>(&original), reinterpret_cast<PVOID>(detour)) != NO_ERROR) {
            ::DetourTransactionAbort();
            return std::unexpected(HookError::DetoursFailed);
        }

        if (::DetourTransactionCommit() != NO_ERROR) {
            return std::unexpected(HookError::DetoursFailed);
        }

        return DetoursHook(original, detour);
    }

    ~DetoursHook() {
        detach();
    }

    DetoursHook(const DetoursHook&) = delete;
    DetoursHook& operator=(const DetoursHook&) = delete;

    DetoursHook(DetoursHook&& other) noexcept
        : original_(std::exchange(other.original_, nullptr)),
          detour_(std::exchange(other.detour_, nullptr)) {}

    DetoursHook& operator=(DetoursHook&& other) noexcept {
        if (this != &other) {
            detach();
            original_ = std::exchange(other.original_, nullptr);
            detour_ = std::exchange(other.detour_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] Func original() const noexcept {
        return original_;
    }

private:
    DetoursHook(Func original, Func detour)
        : original_(original), detour_(detour) {}

    void detach() noexcept {
        if (!original_ || !detour_) {
            return;
        }

        ::DetourTransactionBegin();
        ::DetourUpdateThread(::GetCurrentThread());
        ::DetourDetach(reinterpret_cast<PVOID*>(&original_), reinterpret_cast<PVOID>(detour_));
        ::DetourTransactionCommit();

        original_ = nullptr;
        detour_ = nullptr;
    }

    Func original_ = nullptr;
    Func detour_ = nullptr;
};

} // namespace phantom::hook
