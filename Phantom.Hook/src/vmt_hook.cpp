#include "hook/vmt_hook.h"

#include <utility>

namespace phantom::hook {

const char* hook_error_to_string(HookError error) noexcept {
    switch (error) {
        case HookError::NullPointer:            return "NullPointer";
        case HookError::ProtectionChangeFailed: return "ProtectionChangeFailed";
        case HookError::DetoursFailed:          return "DetoursFailed";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// VMTHook
// ---------------------------------------------------------------------------

VMTHook::VMTHook(void** vtable_entry, void* original, void* detour)
    : vtable_entry_(vtable_entry), original_(original), detour_(detour) {}

std::expected<VMTHook, HookError> VMTHook::create(
    void* instance, size_t index, void* detour) {

    if (!instance || !detour) {
        return std::unexpected(HookError::NullPointer);
    }

    // The first pointer-sized value at *instance is the vtable pointer.
    auto** vtable = *static_cast<void***>(instance);
    void** entry = &vtable[index];
    void* original = vtable[index];

    // Make the vtable entry writable.
    DWORD old_protect{};
    if (!::VirtualProtect(entry, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        return std::unexpected(HookError::ProtectionChangeFailed);
    }

    // Swap in the detour.
    *entry = detour;

    // Restore original page protection.
    DWORD tmp{};
    ::VirtualProtect(entry, sizeof(void*), old_protect, &tmp);

    return VMTHook(entry, original, detour);
}

VMTHook::~VMTHook() {
    restore();
}

VMTHook::VMTHook(VMTHook&& other) noexcept
    : vtable_entry_(std::exchange(other.vtable_entry_, nullptr)),
      original_(std::exchange(other.original_, nullptr)),
      detour_(std::exchange(other.detour_, nullptr)) {}

VMTHook& VMTHook::operator=(VMTHook&& other) noexcept {
    if (this != &other) {
        restore();
        vtable_entry_ = std::exchange(other.vtable_entry_, nullptr);
        original_ = std::exchange(other.original_, nullptr);
        detour_ = std::exchange(other.detour_, nullptr);
    }
    return *this;
}

void* VMTHook::original() const noexcept {
    return original_;
}

void VMTHook::restore() noexcept {
    if (!vtable_entry_) {
        return;
    }

    DWORD old_protect{};
    if (::VirtualProtect(vtable_entry_, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        *vtable_entry_ = original_;

        DWORD tmp{};
        ::VirtualProtect(vtable_entry_, sizeof(void*), old_protect, &tmp);
    }

    vtable_entry_ = nullptr;
    original_ = nullptr;
    detour_ = nullptr;
}

} // namespace phantom::hook
