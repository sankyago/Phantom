#include "launcher/registry_helper.h"

#include <array>

namespace phantom::launcher
{

const char* registry_error_to_string(RegistryError error)
{
    switch (error)
    {
    case RegistryError::KeyNotFound:
        return "Registry key not found";
    case RegistryError::ValueNotFound:
        return "Registry value not found";
    case RegistryError::ReadFailed:
        return "Failed to read registry value";
    case RegistryError::GtaNotFound:
        return "GTA V installation not found in registry";
    default:
        return "Unknown error";
    }
}

std::expected<std::string, RegistryError> RegistryHelper::read_string(HKEY root,
                                                                       std::string_view subkey,
                                                                       std::string_view value_name)
{
    HKEY key_handle = nullptr;
    std::string subkey_str(subkey);

    LONG result = ::RegOpenKeyExA(root, subkey_str.c_str(), 0, KEY_READ, &key_handle);
    if (result != ERROR_SUCCESS)
    {
        return std::unexpected(RegistryError::KeyNotFound);
    }

    // Query size first
    DWORD type = 0;
    DWORD size = 0;
    std::string value_name_str(value_name);

    result = ::RegQueryValueExA(key_handle, value_name_str.c_str(), nullptr, &type, nullptr, &size);
    if (result != ERROR_SUCCESS)
    {
        ::RegCloseKey(key_handle);
        return std::unexpected(RegistryError::ValueNotFound);
    }

    if (type != REG_SZ && type != REG_EXPAND_SZ)
    {
        ::RegCloseKey(key_handle);
        return std::unexpected(RegistryError::ReadFailed);
    }

    std::string buffer(size, '\0');
    result = ::RegQueryValueExA(key_handle, value_name_str.c_str(), nullptr, &type,
                                reinterpret_cast<LPBYTE>(buffer.data()), &size);
    ::RegCloseKey(key_handle);

    if (result != ERROR_SUCCESS)
    {
        return std::unexpected(RegistryError::ReadFailed);
    }

    // Remove trailing null characters
    while (!buffer.empty() && buffer.back() == '\0')
    {
        buffer.pop_back();
    }

    return buffer;
}

std::expected<std::filesystem::path, RegistryError> RegistryHelper::find_gta_install_path()
{
    struct RegistryLocation
    {
        HKEY root;
        const char* subkey;
        const char* value_name;
    };

    // Try common registry locations for GTA V
    static const std::array<RegistryLocation, 4> locations = {{
        {HKEY_LOCAL_MACHINE, R"(SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V)", "InstallFolder"},
        {HKEY_LOCAL_MACHINE, R"(SOFTWARE\Rockstar Games\Grand Theft Auto V)", "InstallFolder"},
        {HKEY_LOCAL_MACHINE, R"(SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{5EFC6C07-6B87-43FC-9524-F9E967241741})", "InstallLocation"},
        {HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 271590)", "InstallLocation"},
    }};

    for (const auto& loc : locations)
    {
        auto result = read_string(loc.root, loc.subkey, loc.value_name);
        if (result.has_value())
        {
            std::filesystem::path path(result.value());
            if (std::filesystem::exists(path))
            {
                return path;
            }
        }
    }

    return std::unexpected(RegistryError::GtaNotFound);
}

} // namespace phantom::launcher
