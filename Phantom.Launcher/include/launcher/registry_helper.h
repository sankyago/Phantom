#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace phantom::launcher
{
enum class RegistryError { KeyNotFound, ValueNotFound, ReadFailed, GtaNotFound };
[[nodiscard]] const char* registry_error_to_string(RegistryError error);

class RegistryHelper
{
  public:
    [[nodiscard]] static std::expected<std::string, RegistryError> read_string(HKEY root,
                                                                               std::string_view subkey,
                                                                               std::string_view value_name);
    [[nodiscard]] static std::expected<std::filesystem::path, RegistryError> find_gta_install_path();
};
} // namespace phantom::launcher
