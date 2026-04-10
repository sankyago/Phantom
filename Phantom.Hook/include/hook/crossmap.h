#pragma once

#include <cstddef>
#include <cstdint>

namespace phantom::hook {

// Stub crossmap -- full data will be populated later from GTA V research.
// Each pair of entries represents {old_hash, new_hash}.
inline constexpr uint64_t CROSSMAP[] = {0, 0};
inline constexpr size_t CROSSMAP_SIZE = sizeof(CROSSMAP) / sizeof(uint64_t);

} // namespace phantom::hook
