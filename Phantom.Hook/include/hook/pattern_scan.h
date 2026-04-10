#pragma once

#include "hook/memory.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

namespace phantom::hook {

enum class ScanError { PatternNotFound, InvalidPattern };

[[nodiscard]] std::string scan_error_to_string(ScanError error);

struct PatternByte {
    uint8_t value = 0;
    bool wildcard = false;
};

class PatternScanner {
public:
    [[nodiscard]] static std::expected<MemoryHandle, ScanError> scan(
        const MemoryRegion& region, std::string_view pattern);

private:
    [[nodiscard]] static std::expected<std::vector<PatternByte>, ScanError> parse_pattern(
        std::string_view pattern);
};

} // namespace phantom::hook
