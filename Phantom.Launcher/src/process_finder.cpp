#include "launcher/process_finder.h"
#include "shared/scoped_handle.h"

#include <TlHelp32.h>
#include <algorithm>
#include <cctype>
#include <cwctype>
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
    ScopedHandle snapshot{::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot)
    {
        return std::unexpected(FindError::SnapshotFailed);
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (!::Process32FirstW(snapshot.get(), &entry))
    {
        return std::unexpected(FindError::ProcessNotFound);
    }

    // Convert target to wide string for comparison
    auto to_lower_w = [](std::wstring s) -> std::wstring {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return s;
    };

    std::wstring target(process_name.begin(), process_name.end());
    target = to_lower_w(target);

    do
    {
        std::wstring current = to_lower_w(entry.szExeFile);
        if (current == target)
        {
            return entry.th32ProcessID;
        }
    } while (::Process32NextW(snapshot.get(), &entry));

    return std::unexpected(FindError::ProcessNotFound);
}

} // namespace phantom::launcher
