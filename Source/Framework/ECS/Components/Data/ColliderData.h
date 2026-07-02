#pragma once
#include "../../../Manager/Collision/CollisionShape.h"

struct ColliderData {
    std::vector<std::shared_ptr<CollisionShape>> m_shapes;
    bool m_isStatic = false;
};