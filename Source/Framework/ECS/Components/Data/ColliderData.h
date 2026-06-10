#pragma once
#include <vector>
#include <SimpleMath.h>
#include <DirectXCollision.h>

enum class ColliderType {
    AABB,
    OBB,
    Sphere
};

struct ColliderShape {
    ColliderType type = ColliderType::AABB;
    DirectX::SimpleMath::Vector3 offset = DirectX::SimpleMath::Vector3(0, 0, 0);
    DirectX::SimpleMath::Vector3 extents = DirectX::SimpleMath::Vector3(0.5f, 0.5f, 0.5f); // For AABB/OBB (Half size)
    float radius = 0.5f; // For Sphere
};

struct ColliderData {
    std::vector<ColliderShape> m_shapes;
    bool m_useModelBounds = false;
    bool m_isTrigger = false;
    bool m_isStatic = false;
};