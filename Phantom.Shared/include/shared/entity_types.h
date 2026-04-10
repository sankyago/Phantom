#pragma once
#include <cstdint>

namespace phantom
{
enum class EntityType : uint8_t { Ped = 0, Vehicle = 1, Object = 2 };

struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; bool operator==(const Vec3&) const = default; };
struct EntityTransform { Vec3 position; Vec3 rotation; bool operator==(const EntityTransform&) const = default; };
struct EntityModel { uint32_t hash = 0; EntityType type = EntityType::Ped; bool operator==(const EntityModel&) const = default; };
} // namespace phantom
