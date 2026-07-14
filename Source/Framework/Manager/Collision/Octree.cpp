#include "../../../Pch.h"
#include "Octree.h"
#include "CollisionShape.h"

using namespace DirectX;

OctreeNode::OctreeNode(const BoundingBox& region, int depth, int maxDepth)
    : m_region(region), m_depth(depth), m_maxDepth(maxDepth) {
}

void OctreeNode::Clear() {
    m_items.clear();
    for (int i = 0; i < 8; ++i) {
        if (m_children[i]) {
            m_children[i]->Clear();
            m_children[i].reset();
        }
    }
}

void OctreeNode::Split() {
    Math::Vector3 center = m_region.Center;
    Math::Vector3 extents = Math::Vector3(m_region.Extents.x * 0.5f, m_region.Extents.y * 0.5f, m_region.Extents.z * 0.5f);

    m_children[0] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3(-extents.x,  extents.y, -extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[1] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3( extents.x,  extents.y, -extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[2] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3(-extents.x,  extents.y,  extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[3] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3( extents.x,  extents.y,  extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[4] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3(-extents.x, -extents.y, -extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[5] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3( extents.x, -extents.y, -extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[6] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3(-extents.x, -extents.y,  extents.z), extents), m_depth + 1, m_maxDepth);
    m_children[7] = std::make_unique<OctreeNode>(BoundingBox(center + Math::Vector3( extents.x, -extents.y,  extents.z), extents), m_depth + 1, m_maxDepth);
}

int OctreeNode::GetChildIndex(const BoundingBox& itemAABB) const {
    Math::Vector3 center = m_region.Center;
    
    bool left = itemAABB.Center.x + itemAABB.Extents.x < center.x;
    bool right = itemAABB.Center.x - itemAABB.Extents.x >= center.x;
    bool bottom = itemAABB.Center.y + itemAABB.Extents.y < center.y;
    bool top = itemAABB.Center.y - itemAABB.Extents.y >= center.y;
    bool front = itemAABB.Center.z + itemAABB.Extents.z < center.z; // -z is front depending on coord system, but let's just use it consistently
    bool back = itemAABB.Center.z - itemAABB.Extents.z >= center.z;

    if (!left && !right) return -1; // Straddles X axis
    if (!bottom && !top) return -1; // Straddles Y axis
    if (!front && !back) return -1; // Straddles Z axis

    int index = 0;
    if (right) index |= 1;
    if (bottom) index |= 4; // Use 4 for bottom
    if (back) index |= 2;   // Use 2 for back

    // The order doesn't strictly matter as long as Split() matches GetChildIndex() logic.
    // Let's ensure consistency:
    // index 0: left, top, front (-x, +y, -z) -> 0
    // index 1: right, top, front (+x, +y, -z) -> 1
    // index 2: left, top, back (-x, +y, +z) -> 2
    // index 3: right, top, back (+x, +y, +z) -> 3
    // index 4: left, bottom, front (-x, -y, -z) -> 4
    // index 5: right, bottom, front (+x, -y, -z) -> 5
    // index 6: left, bottom, back (-x, -y, +z) -> 6
    // index 7: right, bottom, back (+x, -y, +z) -> 7
    return index;
}

void OctreeNode::Insert(const OctreeItem& item) {
    if (m_depth < m_maxDepth) {
        int index = GetChildIndex(item.aabb);
        if (index != -1) {
            if (!m_children[index]) {
                Split();
            }
            m_children[index]->Insert(item);
            return;
        }
    }
    // If we reach here, it either straddles a boundary or we are at max depth
    m_items.push_back(item);
}

void OctreeNode::GetPotentialCollisions(const BoundingBox& testAABB, std::vector<OctreeItem>& outItems) const {
    if (!m_region.Intersects(testAABB)) return;

    for (const auto& item : m_items) {
        if (item.aabb.Intersects(testAABB)) {
            outItems.push_back(item);
        }
    }

    if (m_children[0]) {
        int index = GetChildIndex(testAABB);
        if (index != -1) {
            m_children[index]->GetPotentialCollisions(testAABB, outItems);
        } else {
            // Straddles multiple children, check all intersecting children
            for (int i = 0; i < 8; ++i) {
                if (m_children[i]) {
                    m_children[i]->GetPotentialCollisions(testAABB, outItems);
                }
            }
        }
    }
}

Octree::Octree() {}

void Octree::Build(const BoundingBox& region, int maxDepth) {
    m_root = std::make_unique<OctreeNode>(region, 0, maxDepth);
}

void Octree::Clear() {
    if (m_root) {
        m_root->Clear();
    }
}

void Octree::Insert(const OctreeItem& item) {
    if (m_root && m_root->GetRegion().Intersects(item.aabb)) {
        m_root->Insert(item);
    }
}

void Octree::GetPotentialCollisions(const BoundingBox& testAABB, std::vector<OctreeItem>& outItems) const {
    if (m_root) {
        m_root->GetPotentialCollisions(testAABB, outItems);
    }
}
