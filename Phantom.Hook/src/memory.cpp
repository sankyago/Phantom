#include "hook/memory.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace phantom::hook {

std::string_view memory_error_to_string(MemoryError error) noexcept {
    switch (error) {
        case MemoryError::NullPointer:     return "NullPointer";
        case MemoryError::OutOfBounds:     return "OutOfBounds";
        case MemoryError::ModuleNotFound:  return "ModuleNotFound";
        case MemoryError::InvalidPEHeader: return "InvalidPEHeader";
    }
    return "Unknown";
}

std::expected<Module, MemoryError> Module::from_name(std::string_view name) noexcept {
    // Need null-terminated string for Win32 API
    std::string name_str(name);

    HMODULE hmod = ::GetModuleHandleA(name_str.c_str());
    if (!hmod) {
        return std::unexpected(MemoryError::ModuleNotFound);
    }

    auto base_addr = reinterpret_cast<uintptr_t>(hmod);

    // Parse DOS header
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_addr);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return std::unexpected(MemoryError::InvalidPEHeader);
    }

    // Parse NT headers
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base_addr + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return std::unexpected(MemoryError::InvalidPEHeader);
    }

    auto image_size = static_cast<size_t>(nt->OptionalHeader.SizeOfImage);

    return Module(std::string(name), MemoryHandle(base_addr), image_size);
}

std::expected<MemoryHandle, MemoryError> Module::get_export(std::string_view export_name) const noexcept {
    auto hmod = base_.as<HMODULE>();

    // Need null-terminated string for Win32 API
    std::string export_str(export_name);

    auto* proc = ::GetProcAddress(hmod, export_str.c_str());
    if (!proc) {
        return std::unexpected(MemoryError::ModuleNotFound);
    }

    return MemoryHandle(reinterpret_cast<void*>(proc));
}

} // namespace phantom::hook
