#include "../../../Pch.h"
#include "CollisionShape.h"
#include "../Collision/CollisionManager.h"
#include "../Scene/Scene.h"
#include "../../Object/GameObject.h"


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
        if (auto* pM = ecs.TryGetComponent<ModelRenderData>(m_entity)) {
            auto& m = *pM;
            m_model = m.m_spModelData;
        } else {
            auto scene = CollisionManager::Instance().GetCurrentScene();
            if (scene) {
                auto obj = scene->GetGameObject(m_entity);
                if (obj && obj->GetParent()) {
                    auto parent = obj->GetParent();
                    if (auto* pM = ecs.TryGetComponent<ModelRenderData>(parent->GetEntityID())) {
                        auto& m = *pM;
                        m_model = m.m_spModelData;
                    } else {
                        for (auto& child : parent->GetChildren()) {
                            if (auto* pM = ecs.TryGetComponent<ModelRenderData>(child->GetEntityID())) {
                                auto& m = *pM;
                                m_model = m.m_spModelData;
                                break;
                            }
                        }
                    }
                }
            }
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
    m_worldAABB.Center = wOffset;
    m_worldAABB.Extents = wExtents;
}

void CollisionShapeSphere::UpdateWorldAABB(const Math::Matrix& world) {
}

void CollisionShapeCapsule::UpdateWorldAABB(const Math::Matrix& world) {
}

void CollisionShapeMesh::UpdateWorldAABB(const Math::Matrix& world) {
    if (!m_model && m_entity != INVALID_ENTITY) {
        auto& ecs = GameManager::Instance().GetECS();
        if (auto* pM = ecs.TryGetComponent<ModelRenderData>(m_entity)) {
            auto& m = *pM;
            m_model = m.m_spModelData;
        } else {
            auto scene = CollisionManager::Instance().GetCurrentScene();
            if (scene) {
                auto obj = scene->GetGameObject(m_entity);
                if (obj && obj->GetParent()) {
                    auto parent = obj->GetParent();
                    if (auto* pM = ecs.TryGetComponent<ModelRenderData>(parent->GetEntityID())) {
                        auto& m = *pM;
                        m_model = m.m_spModelData;
                    } else {
                        for (auto& child : parent->GetChildren()) {
                            if (auto* pM = ecs.TryGetComponent<ModelRenderData>(child->GetEntityID())) {
                                auto& m = *pM;
                                m_model = m.m_spModelData;
                                break;
                            }
                        }
                    }
                }
            }
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

void CollisionShapeBox::DrawDebug(const Math::Matrix& worldMatrix, uint32_t color) {
    Math::Vector3 pos = Math::Vector3::Transform(m_offset, worldMatrix);
    Math::Vector3 right = Math::Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13);
    Math::Vector3 up = Math::Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23);
    Math::Vector3 forward = Math::Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33);
    right.Normalize(); up.Normalize(); forward.Normalize();
    
    right *= m_width * 0.5f;
    up *= m_height * 0.5f;
    forward *= m_depth * 0.5f;
    
    Math::Vector3 corners[8] = {
        pos - right - up - forward, pos + right - up - forward,
        pos - right + up - forward, pos + right + up - forward,
        pos - right - up + forward, pos + right - up + forward,
        pos - right + up + forward, pos + right + up + forward,
    };
    
    auto& cm = CollisionManager::Instance();
    cm.AddDebugLine(corners[0], corners[1], color); cm.AddDebugLine(corners[2], corners[3], color);
    cm.AddDebugLine(corners[4], corners[5], color); cm.AddDebugLine(corners[6], corners[7], color);
    cm.AddDebugLine(corners[0], corners[2], color); cm.AddDebugLine(corners[1], corners[3], color);
    cm.AddDebugLine(corners[4], corners[6], color); cm.AddDebugLine(corners[5], corners[7], color);
    cm.AddDebugLine(corners[0], corners[4], color); cm.AddDebugLine(corners[1], corners[5], color);
    cm.AddDebugLine(corners[2], corners[6], color); cm.AddDebugLine(corners[3], corners[7], color);
}


void CollisionShapeSphere::DrawDebug(const Math::Matrix& worldMatrix, uint32_t color) {
    Math::Vector3 pos = Math::Vector3::Transform(m_offset, worldMatrix);
    auto& cm = CollisionManager::Instance();
    
    const int segments = 16;
    for (int i = 0; i < segments; ++i) {
        float t1 = (float)i / segments * 3.14159f * 2.0f;
        float t2 = (float)(i + 1) / segments * 3.14159f * 2.0f;
        cm.AddDebugLine(pos + Math::Vector3(cos(t1), sin(t1), 0)*radius, pos + Math::Vector3(cos(t2), sin(t2), 0)*radius, color);
        cm.AddDebugLine(pos + Math::Vector3(cos(t1), 0, sin(t1))*radius, pos + Math::Vector3(cos(t2), 0, sin(t2))*radius, color);
        cm.AddDebugLine(pos + Math::Vector3(0, cos(t1), sin(t1))*radius, pos + Math::Vector3(0, cos(t2), sin(t2))*radius, color);
    }
}


void CollisionShapeCapsule::DrawDebug(const Math::Matrix& worldMatrix, uint32_t color) {
    Math::Vector3 localOffsetStr = Math::Vector3(0, height * 0.5f, 0);
    Math::Vector3 p1 = Math::Vector3::Transform(m_offset + localOffsetStr, worldMatrix);
    Math::Vector3 p2 = Math::Vector3::Transform(m_offset - localOffsetStr, worldMatrix);
    Math::Vector3 up = p1 - p2;
    if(up.LengthSquared() < 0.001f) up = Math::Vector3(0, 1, 0);
    else up.Normalize();
    Math::Vector3 right = Math::Vector3::TransformNormal(Math::Vector3(1,0,0), worldMatrix);
    if(abs(right.Dot(up)) > 0.99f) right = Math::Vector3(0,0,1);
    right = right - up * right.Dot(up);
    right.Normalize();
    Math::Vector3 forward = up.Cross(right);
    
    auto& cm = CollisionManager::Instance();
    const int segments = 16;
    for (int i = 0; i < segments; ++i) {
        float t1 = (float)i / segments * 3.14159f * 2.0f;
        float t2 = (float)(i + 1) / segments * 3.14159f * 2.0f;
        // Top hemisphere
        if(i < segments/2) {
            cm.AddDebugLine(p1 + right * cos(t1)*radius + up * sin(t1)*radius, p1 + right * cos(t2)*radius + up * sin(t2)*radius, color);
            cm.AddDebugLine(p1 + forward * cos(t1)*radius + up * sin(t1)*radius, p1 + forward * cos(t2)*radius + up * sin(t2)*radius, color);
        }
        // Bottom hemisphere
        if(i >= segments/2) {
            cm.AddDebugLine(p2 + right * cos(t1)*radius + up * sin(t1)*radius, p2 + right * cos(t2)*radius + up * sin(t2)*radius, color);
            cm.AddDebugLine(p2 + forward * cos(t1)*radius + up * sin(t1)*radius, p2 + forward * cos(t2)*radius + up * sin(t2)*radius, color);
        }
        // Horizontal circles
        cm.AddDebugLine(p1 + right * cos(t1)*radius + forward * sin(t1)*radius, p1 + right * cos(t2)*radius + forward * sin(t2)*radius, color);
        cm.AddDebugLine(p2 + right * cos(t1)*radius + forward * sin(t1)*radius, p2 + right * cos(t2)*radius + forward * sin(t2)*radius, color);
    }
    // Connecting lines
    cm.AddDebugLine(p1 + right * radius, p2 + right * radius, color);
    cm.AddDebugLine(p1 - right * radius, p2 - right * radius, color);
    cm.AddDebugLine(p1 + forward * radius, p2 + forward * radius, color);
    cm.AddDebugLine(p1 - forward * radius, p2 - forward * radius, color);
}


void CollisionShapeMesh::DrawDebug(const Math::Matrix& worldMatrix, uint32_t color) {
    // Mesh collider debug draw is too heavy, skip or draw AABB
    Math::Vector3 pos = Math::Vector3::Transform(m_offset, worldMatrix);
    auto& cm = CollisionManager::Instance();
    cm.AddDebugLine(pos, pos + Math::Vector3(0,1,0), color);
}





