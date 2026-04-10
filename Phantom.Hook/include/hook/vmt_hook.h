#pragma once

#include <cstdint>
#include <expected>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// HookError
// ---------------------------------------------------------------------------

enum class HookError {
    NullPointer,
    ProtectionChangeFailed,
    DetoursFailed,
};

[[nodiscard]] const char* hook_error_to_string(HookError error) noexcept;

// ---------------------------------------------------------------------------
// VMTHook
// ---------------------------------------------------------------------------

class VMTHook {
public:
    [[nodiscard]] static std::expected<VMTHook, HookError> create(
        void* instance, size_t index, void* detour);

    ~VMTHook();

    VMTHook(const VMTHook&) = delete;
    VMTHook& operator=(const VMTHook&) = delete;

    VMTHook(VMTHook&& other) noexcept;
    VMTHook& operator=(VMTHook&& other) noexcept;

    [[nodiscard]] void* original() const noexcept;

private:
    VMTHook(void** vtable_entry, void* original, void* detour);

    void restore() noexcept;

    void** vtable_entry_ = nullptr;
    void* original_ = nullptr;
    void* detour_ = nullptr;
};

} // namespace phantom::hook
