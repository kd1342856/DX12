#pragma once
#include <DirectXCollision.h>
#include "../ECS/Entity/Entity.h"
#include "../../../../Library/nlohmann/json.hpp"

class ModelData;

enum CollisionTags {
    Character = 0x01,
    StageObject = 0x02,
    All = 0xFFFFFFFF,
};

class CollisionShape;

struct CollisionResult {
    std::weak_ptr<CollisionShape> other; 
    bool isHit = false;     
    Math::Vector3 pushVector;   
    Math::Vector3 hitPos;    
};

struct RayInfo {
    Math::Vector3 startPos = {};
    Math::Vector3 rayDir = { 0,0,1 };
    float range = 10.0f;
    uint32_t collisionMask = CollisionTags::All; 

    Entity m_ignoreEntity = INVALID_ENTITY; 
};

struct RayResult {
    std::weak_ptr<CollisionShape> other;
    bool isHit = false;
    float distance = 0; 
    Math::Vector3 hitPos = {};    
    Math::Vector3 hitNormal = {}; 
};

class CollisionShape : public std::enable_shared_from_this<CollisionShape> {
public:
    enum ShapeId {
        Box,
        Sphere,
        Capsule,
        Mesh,
    };

    CollisionShape() {}
    CollisionShape(Entity owner) { m_entity = owner; }
    virtual ~CollisionShape() {}

    virtual ShapeId GetShapeId() const = 0;

    virtual bool RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) = 0;

    virtual void Editor_ImGui();

    virtual void Deserialize(const nlohmann::json& jsonObj) = 0;
    virtual void Serialize(nlohmann::json& outJson) const = 0;

    std::string m_name = "No Attach";

    Entity m_entity = INVALID_ENTITY;

    Math::Vector3 m_offset = {};

    uint32_t m_tags = CollisionTags::All;

    DirectX::BoundingBox m_worldAABB;

    bool m_isTrigger = false;

    virtual void UpdateWorldAABB(const Math::Matrix& world) = 0;
};

class CollisionShapeBox : public CollisionShape {
public:
    CollisionShapeBox() { m_name = "Box"; }
    ShapeId GetShapeId() const override { return Box; }

    void UpdateWorldAABB(const Math::Matrix& world) override;
    bool RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) override;

    void Deserialize(const nlohmann::json& jsonObj) override;
    void Serialize(nlohmann::json& outJson) const override;

    float m_width = 1.0f;
    float m_height = 1.0f;
    float m_depth = 1.0f;

    void Editor_ImGui() override;
};

class CollisionShapeSphere : public CollisionShape {
public:
    CollisionShapeSphere() { m_name = "Sphere"; }
    ShapeId GetShapeId() const override { return Sphere; }

    void UpdateWorldAABB(const Math::Matrix& world) override;
    bool RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) override;

    void Deserialize(const nlohmann::json& jsonObj) override;
    void Serialize(nlohmann::json& outJson) const override;

    float radius = 1.0f;

    void Editor_ImGui() override;
};

class CollisionShapeCapsule : public CollisionShape {
public:
    CollisionShapeCapsule() { m_name = "Capsule"; }
    ShapeId GetShapeId() const override { return Capsule; }

    void UpdateWorldAABB(const Math::Matrix& world) override;
    bool RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) override;

    void Deserialize(const nlohmann::json& jsonObj) override;
    void Serialize(nlohmann::json& outJson) const override;

    float radius = 1.0f; 
    float height = 1.0f; 

    void Editor_ImGui() override;
};

class CollisionShapeMesh : public CollisionShape {
public:
    CollisionShapeMesh() { m_name = "Mesh"; }
    ShapeId GetShapeId() const override { return Mesh; }

    void UpdateWorldAABB(const Math::Matrix& world) override;
    bool RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) override;

    void Deserialize(const nlohmann::json& jsonObj) override;
    void Serialize(nlohmann::json& outJson) const override;

    std::shared_ptr<ModelData> m_model; 
    bool m_isBackfaceCulling = true;  

    void Editor_ImGui() override;
};