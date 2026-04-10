#pragma once
#include <expected>
#include <filesystem>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace phantom::launcher
{
enum class InjectionError {
    ProcessOpenFailed,
    AllocFailed,
    WriteFailed,
    ThreadCreationFailed,
    InjectionTimeout,
    DllNotFound
};
[[nodiscard]] const char* injection_error_to_string(InjectionError error);

class Injector
{
  public:
    [[nodiscard]] static std::expected<void, InjectionError> inject(DWORD process_id,
                                                                     const std::filesystem::path& dll_path);
};
} // namespace phantom::launcher
