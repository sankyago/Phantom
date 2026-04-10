#include "hook/natives.h"
#include "hook/crossmap.h"

#include <cstdint>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// NativeError
// ---------------------------------------------------------------------------

const char* native_error_to_string(NativeError error) {
    switch (error) {
    case NativeError::NotInitialized:  return "NativeInvoker not initialized";
    case NativeError::NativeNotFound:  return "Native handler not found";
    case NativeError::InvocationFailed: return "Native invocation failed";
    }
    return "Unknown NativeError";
}

// ---------------------------------------------------------------------------
// NativeEntry — layout of each entry in the game's registration table
// ---------------------------------------------------------------------------

struct NativeEntry {
    uint64_t hash;
    NativeHandler handler;
};

// ---------------------------------------------------------------------------
// Crossmap translation
// ---------------------------------------------------------------------------

/// Translate a native hash through the crossmap. The crossmap is stored as
/// sequential {old_hash, new_hash} pairs. If the hash is not found in the
/// crossmap the original hash is returned unchanged.
static uint64_t translate_hash(uint64_t hash) {
    for (size_t i = 0; i + 1 < CROSSMAP_SIZE; i += 2) {
        if (CROSSMAP[i] == hash)
            return CROSSMAP[i + 1];
    }
    return hash;
}

// ---------------------------------------------------------------------------
// NativeInvoker
// ---------------------------------------------------------------------------

NativeInvoker::NativeInvoker(MemoryHandle table, size_t size)
    : table_(table), table_size_(size) {}

std::expected<NativeInvoker, NativeError>
NativeInvoker::create(MemoryHandle registration_table, size_t table_size) {
    if (!registration_table.valid())
        return std::unexpected(NativeError::NotInitialized);

    return NativeInvoker(registration_table, table_size);
}

std::expected<NativeHandler, NativeError>
NativeInvoker::find_native(uint64_t hash) const {
    if (!table_.valid())
        return std::unexpected(NativeError::NotInitialized);

    uint64_t translated = translate_hash(hash);

    auto* entries = table_.as<const NativeEntry*>();
    for (size_t i = 0; i < table_size_; ++i) {
        if (entries[i].hash == translated)
            return entries[i].handler;
    }

    return std::unexpected(NativeError::NativeNotFound);
}

} // namespace phantom::hook
