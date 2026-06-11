#include "CollisionShape.h"
#include "../Manager/GameManager.h"
#include "../ECS/Components/Data/ModelRenderData.h"
#include <algorithm>

#include "../../Graphics/Geometry/Model/Model.h"
#include "../../Graphics/Geometry/Mesh/Mesh.h"

void CollisionShape::Editor_ImGui() {
    ImGui::DragFloat3("Offset", &m_offset.x, 0.01f);

    if (ImGui::TreeNode("Tags")) {
        int tags = (int)m_tags;
        if (ImGui::InputInt("##Tag", &tags)) {
            m_tags = (uint32_t)tags;
        }
        ImGui::TreePop();
    }
}

void CollisionShapeBox::Deserialize(const nlohmann::json& jsonObj) {
    if (jsonObj.contains("Offset")) {
        auto& offsetArr = jsonObj["Offset"];
        if (offsetArr.is_array() && offsetArr.size() >= 3) {
            m_offset = Math::Vector3(offsetArr[0], offsetArr[1], offsetArr[2]);
        }
    }
    if (jsonObj.contains("Tags")) m_tags = jsonObj["Tags"];
    if (jsonObj.contains("Width")) m_width = jsonObj["Width"];
    if (jsonObj.contains("Height")) m_height = jsonObj["Height"];
    if (jsonObj.contains("Depth")) m_depth = jsonObj["Depth"];
}

void CollisionShapeBox::Serialize(nlohmann::json& outJson) const {
    outJson["Offset"] = { m_offset.x, m_offset.y, m_offset.z };
    outJson["Tags"] = m_tags;
    outJson["Width"] = m_width;
    outJson["Height"] = m_height;
    outJson["Depth"] = m_depth;
}

void CollisionShapeBox::Editor_ImGui() {
    CollisionShape::Editor_ImGui();
    if (ImGui::DragFloat("Width", &m_width, 0.01f)) {
        if (m_width < 0.01f) m_width = 0.01f;
    }
    if (ImGui::DragFloat("Height", &m_height, 0.01f)) {
        if (m_height < 0.01f) m_height = 0.01f;
    }
    if (ImGui::DragFloat("Depth", &m_depth, 0.01f)) {
        if (m_depth < 0.01f) m_depth = 0.01f;
    }
}

void CollisionShapeSphere::Deserialize(const nlohmann::json& jsonObj) {
    if (jsonObj.contains("Offset")) {
        auto& offsetArr = jsonObj["Offset"];
        if (offsetArr.is_array() && offsetArr.size() >= 3) {
            m_offset = Math::Vector3(offsetArr[0], offsetArr[1], offsetArr[2]);
        }
    }
    if (jsonObj.contains("Tags")) m_tags = jsonObj["Tags"];
    if (jsonObj.contains("Radius")) radius = jsonObj["Radius"];
}

void CollisionShapeSphere::Serialize(nlohmann::json& outJson) const {
    outJson["Offset"] = { m_offset.x, m_offset.y, m_offset.z };
    outJson["Tags"] = m_tags;
    outJson["Radius"] = radius;
}

void CollisionShapeSphere::Editor_ImGui() {
    CollisionShape::Editor_ImGui();
    if (ImGui::DragFloat("Radius", &radius, 0.01f)) {
        if (radius < 0.01f) radius = 0.01f;
    }
}

void CollisionShapeCapsule::Deserialize(const nlohmann::json& jsonObj) {
    if (jsonObj.contains("Offset")) {
        auto& offsetArr = jsonObj["Offset"];
        if (offsetArr.is_array() && offsetArr.size() >= 3) {
            m_offset = Math::Vector3(offsetArr[0], offsetArr[1], offsetArr[2]);
        }
    }
    if (jsonObj.contains("Tags")) m_tags = jsonObj["Tags"];
    if (jsonObj.contains("Radius")) radius = jsonObj["Radius"];
    if (jsonObj.contains("Height")) height = jsonObj["Height"];
}

void CollisionShapeCapsule::Serialize(nlohmann::json& outJson) const {
    outJson["Offset"] = { m_offset.x, m_offset.y, m_offset.z };
    outJson["Tags"] = m_tags;
    outJson["Radius"] = radius;
    outJson["Height"] = height;
}

void CollisionShapeCapsule::Editor_ImGui() {
    CollisionShape::Editor_ImGui();
    if (ImGui::DragFloat("Radius", &radius, 0.01f)) {
        if (radius < 0.01f) radius = 0.01f;
    }
    if (ImGui::DragFloat("Height", &height, 0.01f)) {
        if (height < 0.01f) height = 0.01f;
    }
}

void CollisionShapeMesh::Deserialize(const nlohmann::json& jsonObj) {
    if (jsonObj.contains("Offset")) {
        auto& offsetArr = jsonObj["Offset"];
        if (offsetArr.is_array() && offsetArr.size() >= 3) {
            m_offset = Math::Vector3(offsetArr[0], offsetArr[1], offsetArr[2]);
        }
    }
    if (jsonObj.contains("Tags")) m_tags = jsonObj["Tags"];
}

void CollisionShapeMesh::Serialize(nlohmann::json& outJson) const {
    outJson["Offset"] = { m_offset.x, m_offset.y, m_offset.z };
    outJson["Tags"] = m_tags;
}

void CollisionShapeMesh::Editor_ImGui() {
    CollisionShape::Editor_ImGui();
}

bool CollisionShapeBox::RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) {
    Math::Matrix invWorld = world.Invert();
    Math::Vector3 localStart = Math::Vector3::Transform(ray.startPos, invWorld) - m_offset;
    Math::Vector3 localDir = Math::Vector3::TransformNormal(ray.rayDir, invWorld);
    localDir.Normalize();
    
    DirectX::BoundingBox box(Math::Vector3::Zero, Math::Vector3(m_width * 0.5f, m_height * 0.5f, m_depth * 0.5f));
    DirectX::XMVECTOR s = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&localStart));
    DirectX::XMVECTOR d = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&localDir));
    float dOut = 0;
    if (box.Intersects(s, d, dOut)) {
        if (dOut <= ray.range) {
            out.isHit = true;
            out.distance = dOut;
            out.hitPos = ray.startPos + ray.rayDir * dOut;
            return true;
        }
    }
    return false;
}

bool CollisionShapeSphere::RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) {
    Math::Vector3 wOffset = Math::Vector3::Transform(m_offset, world);
    DirectX::BoundingSphere sphere(wOffset, radius);
    DirectX::XMVECTOR s = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&ray.startPos));
    DirectX::XMVECTOR d = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&ray.rayDir));
    float dOut = 0;
    if (sphere.Intersects(s, d, dOut)) {
        if (dOut <= ray.range) {
            out.isHit = true;
            out.distance = dOut;
            out.hitPos = ray.startPos + ray.rayDir * dOut;
            return true;
        }
    }
    return false;
}

bool CollisionShapeCapsule::RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) {
    Math::Vector3 localOffsetStr = Math::Vector3(0, height * 0.5f, 0);
    Math::Vector3 p1 = Math::Vector3::Transform(m_offset + localOffsetStr, world);
    Math::Vector3 p2 = Math::Vector3::Transform(m_offset - localOffsetStr, world);
    
    return false;
}

bool CollisionShapeMesh::RayCast(const RayInfo& ray, const Math::Matrix& world, RayResult& out) {
    if (!m_model && m_entity != INVALID_ENTITY) {
        auto& ecs = GameManager::Instance().GetECS();
        if (ecs.HasComponent<ModelRenderData>(m_entity)) {
            m_model = ecs.GetComponent<ModelRenderData>(m_entity).m_spModelData;
        }
    }
    if (!m_model) return false;
    
    bool hit = false;
    float minDist = ray.range;
    Math::Matrix invWorld = world.Invert();
    Math::Vector3 localStart = Math::Vector3::Transform(ray.startPos, invWorld) - m_offset;
    Math::Vector3 localDir = Math::Vector3::TransformNormal(ray.rayDir, invWorld);
    localDir.Normalize();
    
    DirectX::XMVECTOR s = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&localStart));
    DirectX::XMVECTOR d = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&localDir));
    
    for (const auto& node : m_model->GetNodes()) {
        aiMatrix4x4 aiMat = node.localTransform;
        aiMat.Transpose();
        Math::Matrix nodeLocal = *(Math::Matrix*)&aiMat;
        DirectX::XMMATRIX mNode = DirectX::XMLoadFloat4x4(&nodeLocal);
        DirectX::XMVECTOR sNode = DirectX::XMVector3Transform(s, DirectX::XMMatrixInverse(nullptr, mNode));
        DirectX::XMVECTOR dNode = DirectX::XMVector3TransformNormal(d, DirectX::XMMatrixInverse(nullptr, mNode));
        
        for (const auto& mesh : node.meshes) {
            if (!mesh) continue;
            const auto& verts = mesh->GetVertices();
            for (const auto& face : mesh->GetFaces()) {
                DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&verts[face.Idx[0]].Position));
                DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&verts[face.Idx[1]].Position));
                DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&verts[face.Idx[2]].Position));
                
                float dOut = 0;
                if (DirectX::TriangleTests::Intersects(sNode, dNode, v0, v1, v2, dOut)) {
                    if (dOut < minDist) {
                        minDist = dOut;
                        hit = true;
                    }
                }
            }
        }
    }
    
    if (hit) {
        out.distance = minDist;
        out.isHit = true;
        out.hitPos = ray.startPos + ray.rayDir * minDist;
        return true;
    }
    return false;
}

void CollisionShapeBox::UpdateWorldAABB(const Math::Matrix& world) {
    Math::Vector3 wOffset = Math::Vector3::Transform(m_offset, world);
    Math::Vector3 right = Math::Vector3(world._11, world._12, world._13);
    Math::Vector3 up = Math::Vector3(world._21, world._22, world._23);
    Math::Vector3 forward = Math::Vector3(world._31, world._32, world._33);
    Math::Vector3 extents = Math::Vector3(m_width, m_height, m_depth) * 0.5f;
    Math::Vector3 wExtents = Math::Vector3(
        std::abs(right.x) * extents.x + std::abs(up.x) * extents.y + std::abs(forward.x) * extents.z,
        std::abs(right.y) * extents.x + std::abs(up.y) * extents.y + std::abs(forward.y) * extents.z,
        std::abs(right.z) * extents.x + std::abs(up.z) * extents.y + std::abs(forward.z) * extents.z
    );
    // Note: WorldAABB would be updated in the system if needed, or stored in shape if needed
    // The previous architecture used to update m_worldAABB in KdCollider
}

void CollisionShapeSphere::UpdateWorldAABB(const Math::Matrix& world) {
}

void CollisionShapeCapsule::UpdateWorldAABB(const Math::Matrix& world) {
}

void CollisionShapeMesh::UpdateWorldAABB(const Math::Matrix& world) {
    if (!m_model && m_entity != INVALID_ENTITY) {
        auto& ecs = GameManager::Instance().GetECS();
        if (ecs.HasComponent<ModelRenderData>(m_entity)) {
            m_model = ecs.GetComponent<ModelRenderData>(m_entity).m_spModelData;
        }
    }

    if (!m_model) {
        m_worldAABB.Extents = Math::Vector3(0, 0, 0);
        return;
    }

    Math::Vector3 aabbMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Math::Vector3 aabbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    Math::Matrix worldMat = Math::Matrix::CreateTranslation(m_offset) * world;

    for (const auto& node : m_model->GetNodes()) {
        for (const auto& mesh : node.meshes) {
            for (const auto& vert : mesh->GetVertices()) {
                Math::Vector3 wp = Math::Vector3::Transform(vert.Position, worldMat);
                aabbMin = Math::Vector3::Min(aabbMin, wp);
                aabbMax = Math::Vector3::Max(aabbMax, wp);
            }
        }
    }

    if (aabbMin.x < FLT_MAX) {
        Math::Vector3 center = (aabbMin + aabbMax) * 0.5f;
        Math::Vector3 extents = (aabbMax - aabbMin) * 0.5f;
        m_worldAABB.Center = center;
        m_worldAABB.Extents = extents;
    } else {
        m_worldAABB.Extents = Math::Vector3(0, 0, 0);
    }
}