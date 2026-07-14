#pragma once
#include <DirectXCollision.h>
#include <vector>
#include <memory>
#include "../../ECS/ECS.h"

class CollisionShape;

struct OctreeItem {
    Entity entity = INVALID_ENTITY;
    CollisionShape* shape = nullptr;
    DirectX::BoundingBox aabb;
};

class OctreeNode {
public:
    OctreeNode(const DirectX::BoundingBox& region, int depth, int maxDepth);
    ~OctreeNode() = default;

    void Clear();
    void Insert(const OctreeItem& item);
    void GetPotentialCollisions(const DirectX::BoundingBox& testAABB, std::vector<OctreeItem>& outItems) const;
    const DirectX::BoundingBox& GetRegion() const { return m_region; }

private:
    DirectX::BoundingBox m_region;
    int m_depth;
    int m_maxDepth;
    std::vector<OctreeItem> m_items;
    std::unique_ptr<OctreeNode> m_children[8];

    void Split();
    int GetChildIndex(const DirectX::BoundingBox& itemAABB) const;
};

class Octree {
public:
    Octree();
    ~Octree() = default;

    void Build(const DirectX::BoundingBox& region, int maxDepth = 5);
    void Clear();
    void Insert(const OctreeItem& item);
    void GetPotentialCollisions(const DirectX::BoundingBox& testAABB, std::vector<OctreeItem>& outItems) const;

private:
    std::unique_ptr<OctreeNode> m_root;
};
