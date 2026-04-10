#pragma once

#include "hook/natives.h"
#include "shared/entity_types.h"

#include <cstdint>
#include <expected>
#include <vector>

namespace phantom::client {

// ---------------------------------------------------------------------------
// EntityError
// ---------------------------------------------------------------------------

enum class EntityError { SpawnFailed, NotFound, NativeCallFailed };

[[nodiscard]] const char* entity_error_to_string(EntityError error);

// ---------------------------------------------------------------------------
// EntityManager
// ---------------------------------------------------------------------------

class EntityManager {
public:
    explicit EntityManager(const hook::NativeInvoker& invoker);

    [[nodiscard]] std::expected<int32_t, EntityError>
    spawn_vehicle(uint32_t model_hash, const Vec3& position, float heading);

    [[nodiscard]] std::expected<int32_t, EntityError>
    spawn_ped(uint32_t model_hash, const Vec3& position, float heading);

    [[nodiscard]] std::expected<void, EntityError>
    delete_entity(int32_t handle);

    void cleanup_all();

    [[nodiscard]] const std::vector<int32_t>& tracked_entities() const noexcept {
        return entities_;
    }

private:
    const hook::NativeInvoker& invoker_;
    std::vector<int32_t> entities_;
};

} // namespace phantom::client
