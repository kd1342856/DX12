#include "../../../../Pch.h"
#include "ReflectionComponent.h"
#include "../../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../../Framework/Manager/GameManager.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../Player/Player.h"

REGISTER_COMPONENT(ReflectionComponent);

void ReflectionComponent::Serialize(nlohmann::json& out) const
{
    out["planeNormal"] = { m_planeNormal.x, m_planeNormal.y, m_planeNormal.z };
    out["planePoint"] = { m_planePoint.x, m_planePoint.y, m_planePoint.z };
    out["debugSize"] = m_debugSize;
    out["roomName"] = m_roomName;
    out["activationDistance"] = m_activationDistance;
}

void ReflectionComponent::Deserialize(const nlohmann::json& in)
{
    if (in.contains("planeNormal")) {
        auto arr = in["planeNormal"];
        m_planeNormal = { arr[0], arr[1], arr[2] };
    }
    if (in.contains("planePoint")) {
        auto arr = in["planePoint"];
        m_planePoint = { arr[0], arr[1], arr[2] };
    }
    if (in.contains("debugSize")) {
        m_debugSize = in["debugSize"];
    }
    if (in.contains("roomName")) {
        m_roomName = in["roomName"].get<std::string>();
    }
    if (in.contains("activationDistance")) {
        m_activationDistance = in["activationDistance"];
    }
}

void ReflectionComponent::ImGuiUpdate()
{
    ImGui::Text("Reflection Plane");
    ImGui::DragFloat3("Normal", &m_planeNormal.x, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat3("Point", &m_planePoint.x, 0.1f);
    ImGui::DragFloat("Debug Size", &m_debugSize, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Activation Distance", &m_activationDistance, 0.1f, 0.5f, 50.0f);
    ImGui::TextDisabled("Reflection is active while the player is within this distance of the mirror plane.");

    // Normal must always stay unit length
    m_planeNormal.Normalize();

    ImGui::Text("Active: %s", m_isActive ? "true" : "false");
}

void ReflectionComponent::Update(float deltaTime)
{
    auto& ecs = GameManager::Instance().GetECS();
    Player* pPlayer = nullptr;
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* pCandidate = dynamic_cast<Player*>(scriptData.Instance.get())) {
            pPlayer = pCandidate;
            break;
        }
    }

    if (!pPlayer || !pPlayer->GetGameObject()) {
        m_isActive = false;
        return;
    }

    auto* cPlayerTrans = ecs.TryGetComponent<TransformData>(pPlayer->GetGameObject()->GetEntityID());
    if (!cPlayerTrans) {
        m_isActive = false;
        return;
    }

    // Compute the world-space plane point right here rather than relying on PreDraw()'s cached
    // m_worldPlanePoint: Update() runs before PreDraw() each frame, so reading the cached value
    // here would always be one frame stale (harmless for a static mirror, but wrong in general).
    Math::Vector3 worldPlanePoint = m_planePoint;
    if (m_pGameObject) {
        if (auto* cTrans = ecs.TryGetComponent<TransformData>(m_pGameObject->GetEntityID())) {
            worldPlanePoint = Math::Vector3::Transform(m_planePoint, cTrans->m_worldMatrix);
        }
    }

    float distSq = Math::Vector3::DistanceSquared(cPlayerTrans->m_position, worldPlanePoint);
    m_isActive = distSq <= (m_activationDistance * m_activationDistance);
}

void ReflectionComponent::PreDraw()
{
    auto& ecs = GameManager::Instance().GetECS();
    if (m_pGameObject) {
        if (auto* cTrans = ecs.TryGetComponent<TransformData>(m_pGameObject->GetEntityID())) {
            m_worldPlanePoint = Math::Vector3::Transform(m_planePoint, cTrans->m_worldMatrix);
            m_worldPlaneNormal = Math::Vector3::TransformNormal(m_planeNormal, cTrans->m_worldMatrix);
            m_worldPlaneNormal.Normalize();
        } else {
            m_worldPlanePoint = m_planePoint;
            m_worldPlaneNormal = m_planeNormal;
        }
    } else {
        m_worldPlanePoint = m_planePoint;
        m_worldPlaneNormal = m_planeNormal;
    }

    {
        Math::Vector3 up = (std::abs(m_worldPlaneNormal.y) > 0.9f) ? Math::Vector3(1, 0, 0) : Math::Vector3(0, 1, 0);
        Math::Vector3 right = m_worldPlaneNormal.Cross(up);
        right.Normalize();
        up = right.Cross(m_worldPlaneNormal);
        up.Normalize();

        Math::Vector3 extR = right * (m_debugSize * 0.5f);
        Math::Vector3 extU = up * (m_debugSize * 0.5f);

        Math::Vector3 p0 = m_worldPlanePoint - extR - extU;
        Math::Vector3 p1 = m_worldPlanePoint + extR - extU;
        Math::Vector3 p2 = m_worldPlanePoint + extR + extU;
        Math::Vector3 p3 = m_worldPlanePoint - extR + extU;

        // Active -> green, inactive -> gray, so it's easy to spot at a glance
        ImU32 colYellow = m_isActive ? IM_COL32(80, 255, 80, 255) : IM_COL32(150, 150, 150, 150);
        ImU32 colRed = IM_COL32(255, 0, 0, 255);

        CollisionManager::Instance().AddDebugLine(p0, p1, colYellow);
        CollisionManager::Instance().AddDebugLine(p1, p2, colYellow);
        CollisionManager::Instance().AddDebugLine(p2, p3, colYellow);
        CollisionManager::Instance().AddDebugLine(p3, p0, colYellow);

        CollisionManager::Instance().AddDebugLine(m_worldPlanePoint, m_worldPlanePoint + m_worldPlaneNormal * 1.0f, colRed);
    }
}
