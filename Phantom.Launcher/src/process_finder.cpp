#include "launcher/process_finder.h"
#include "hook/scoped_handle.h"

#include <TlHelp32.h>
#include <algorithm>
#include <cctype>
#include <string>

namespace phantom::launcher
{

const char* find_error_to_string(FindError error)
{
    switch (error)
    {
    case FindError::ProcessNotFound:
        return "Process not found";
    case FindError::SnapshotFailed:
        return "Failed to create toolhelp snapshot";
    default:
        return "Unknown error";
    }
}

std::expected<DWORD, FindError> ProcessFinder::find_by_name(std::string_view process_name)
{
    hook::ScopedHandle snapshot{::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot)
    {
        return std::unexpected(FindError::SnapshotFailed);
    }

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (!::Process32First(snapshot.get(), &entry))
    {
        return std::unexpected(FindError::ProcessNotFound);
    }

    // Case-insensitive comparison helper
    auto to_lower = [](std::string s) -> std::string {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };

    std::string target = to_lower(std::string(process_name));

    do
    {
        std::string current = to_lower(entry.szExeFile);
        if (current == target)
        {
            return entry.th32ProcessID;
        }
    } while (::Process32Next(snapshot.get(), &entry));

    return std::unexpected(FindError::ProcessNotFound);
}

} // namespace phantom::launcher
