#pragma once
#include <expected>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace phantom::launcher
{

enum class RegistryError { KeyNotFound, ValueNotFound, ReadFailed, WriteFailed };
const char* registry_error_to_string(RegistryError error);

class RegistryStorage
{
  public:
    static std::expected<std::string, RegistryError> get(HKEY root, std::string_view subkey,
                                                        std::string_view value_name);

    static std::expected<void, RegistryError> set(HKEY root, std::string_view subkey,
                                                   std::string_view value_name,
                                                   std::string_view value);
};

} // namespace phantom::launcher
