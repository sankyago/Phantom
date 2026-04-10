#include "client/entities/entity_manager.h"

#include <algorithm>

namespace phantom::client {

// ---------------------------------------------------------------------------
// entity_error_to_string
// ---------------------------------------------------------------------------

const char* entity_error_to_string(EntityError error) {
    switch (error) {
    case EntityError::SpawnFailed:      return "SpawnFailed";
    case EntityError::NotFound:         return "NotFound";
    case EntityError::NativeCallFailed: return "NativeCallFailed";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// EntityManager
// ---------------------------------------------------------------------------

EntityManager::EntityManager(const hook::NativeInvoker& invoker)
    : invoker_(invoker) {}

std::expected<int32_t, EntityError>
EntityManager::spawn_vehicle(uint32_t model_hash, const Vec3& position, float heading) {
    // CREATE_VEHICLE(modelHash, x, y, z, heading, isNetwork, bScriptHostVeh)
    auto result = invoker_.invoke<int32_t>(
        0xAF35D0D2583051B0,
        model_hash,
        position.x, position.y, position.z,
        heading,
        false, false);

    if (!result.has_value())
        return std::unexpected(EntityError::NativeCallFailed);

    int32_t handle = result.value();
    if (handle == 0)
        return std::unexpected(EntityError::SpawnFailed);

    entities_.push_back(handle);
    return handle;
}

std::expected<int32_t, EntityError>
EntityManager::spawn_ped(uint32_t model_hash, const Vec3& position, float heading) {
    // CREATE_PED(pedType, modelHash, x, y, z, heading, isNetwork, bScriptHostVeh)
    auto result = invoker_.invoke<int32_t>(
        0xD49F9B0955C367DE,
        static_cast<int32_t>(26),  // pedType = 26 (multiplayer)
        model_hash,
        position.x, position.y, position.z,
        heading,
        false, false);

    if (!result.has_value())
        return std::unexpected(EntityError::NativeCallFailed);

    int32_t handle = result.value();
    if (handle == 0)
        return std::unexpected(EntityError::SpawnFailed);

    entities_.push_back(handle);
    return handle;
}

std::expected<void, EntityError>
EntityManager::delete_entity(int32_t handle) {
    auto it = std::find(entities_.begin(), entities_.end(), handle);
    if (it == entities_.end())
        return std::unexpected(EntityError::NotFound);

    // DELETE_ENTITY(&handle)
    auto result = invoker_.invoke_void(0xAE3CBE5BF394C9C9, &handle);
    if (!result.has_value())
        return std::unexpected(EntityError::NativeCallFailed);

    entities_.erase(it);
    return {};
}

void EntityManager::cleanup_all() {
    auto copy = entities_;
    for (auto handle : copy) {
        // Best-effort deletion — ignore individual failures
        (void)invoker_.invoke_void(0xAE3CBE5BF394C9C9, &handle);
    }
    entities_.clear();
}

} // namespace phantom::client
