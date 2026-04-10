#include "hook/pattern_scan.h"

#include <charconv>

namespace phantom::hook {

std::string scan_error_to_string(ScanError error) {
    switch (error) {
        case ScanError::PatternNotFound: return "PatternNotFound";
        case ScanError::InvalidPattern:  return "InvalidPattern";
    }
    return "Unknown";
}

std::expected<std::vector<PatternByte>, ScanError> PatternScanner::parse_pattern(
    std::string_view pattern) {
    std::vector<PatternByte> bytes;

    size_t pos = 0;
    while (pos < pattern.size()) {
        // Skip leading spaces
        while (pos < pattern.size() && pattern[pos] == ' ')
            ++pos;

        if (pos >= pattern.size())
            break;

        // Find end of token
        size_t start = pos;
        while (pos < pattern.size() && pattern[pos] != ' ')
            ++pos;

        std::string_view token = pattern.substr(start, pos - start);

        if (token == "?" || token == "??") {
            bytes.push_back({0, true});
        } else {
            uint8_t value = 0;
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value, 16);
            if (ec != std::errc{} || ptr != token.data() + token.size()) {
                return std::unexpected(ScanError::InvalidPattern);
            }
            bytes.push_back({value, false});
        }
    }

    if (bytes.empty()) {
        return std::unexpected(ScanError::InvalidPattern);
    }

    return bytes;
}

std::expected<MemoryHandle, ScanError> PatternScanner::scan(
    const MemoryRegion& region, std::string_view pattern) {
    auto parsed = parse_pattern(pattern);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    const auto& bytes = parsed.value();
    auto data = region.as_bytes();

    if (bytes.size() > data.size()) {
        return std::unexpected(ScanError::PatternNotFound);
    }

    size_t scan_end = data.size() - bytes.size() + 1;
    for (size_t i = 0; i < scan_end; ++i) {
        bool match = true;
        for (size_t j = 0; j < bytes.size(); ++j) {
            if (!bytes[j].wildcard && data[i + j] != bytes[j].value) {
                match = false;
                break;
            }
        }
        if (match) {
            return MemoryHandle(region.base().address() + i);
        }
    }

    return std::unexpected(ScanError::PatternNotFound);
}

} // namespace phantom::hook
