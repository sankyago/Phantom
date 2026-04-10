#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace phantom::launcher
{
enum class FindError { ProcessNotFound, SnapshotFailed };
[[nodiscard]] const char* find_error_to_string(FindError error);

class ProcessFinder
{
  public:
    [[nodiscard]] static std::expected<DWORD, FindError> find_by_name(std::string_view process_name);
};
} // namespace phantom::launcher
