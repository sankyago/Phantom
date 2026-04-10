#include "launcher/registry_storage.h"

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
    case RegistryError::WriteFailed:
        return "Failed to write registry value";
    default:
        return "Unknown error";
    }
}

std::expected<std::string, RegistryError> RegistryStorage::get(HKEY root, std::string_view subkey,
                                                                std::string_view value_name)
{
    HKEY key_handle = nullptr;
    std::string subkey_str(subkey);

    LONG result = ::RegOpenKeyExA(root, subkey_str.c_str(), 0, KEY_READ, &key_handle);
    if (result != ERROR_SUCCESS)
    {
        return std::unexpected(RegistryError::KeyNotFound);
    }

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

    while (!buffer.empty() && buffer.back() == '\0')
    {
        buffer.pop_back();
    }

    return buffer;
}

std::expected<void, RegistryError> RegistryStorage::set(HKEY root, std::string_view subkey,
                                                         std::string_view value_name, std::string_view value)
{
    HKEY key_handle = nullptr;
    std::string subkey_str(subkey);

    LONG result = ::RegCreateKeyExA(root, subkey_str.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                    &key_handle, nullptr);
    if (result != ERROR_SUCCESS)
    {
        return std::unexpected(RegistryError::KeyNotFound);
    }

    std::string value_name_str(value_name);
    result = ::RegSetValueExA(key_handle, value_name_str.c_str(), 0, REG_SZ,
                              reinterpret_cast<const BYTE*>(value.data()),
                              static_cast<DWORD>(value.size() + 1));
    ::RegCloseKey(key_handle);

    if (result != ERROR_SUCCESS)
    {
        return std::unexpected(RegistryError::WriteFailed);
    }

    return {};
}

} // namespace phantom::launcher
