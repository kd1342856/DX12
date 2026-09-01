#include "../../../../Pch.h"
#include "RoomArea.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/Manager/Collision/CollisionManager.h"
#include "../Ghost/GhostAI.h"

REGISTER_COMPONENT(RoomArea);

void RoomArea::Serialize(nlohmann::json& out) const
{
    out["minX"] = m_min.x;
    out["minY"] = m_min.y;
    out["minZ"] = m_min.z;
    out["maxX"] = m_max.x;
    out["maxY"] = m_max.y;
    out["maxZ"] = m_max.z;
    out["isCandidate"] = m_isGhostRoomCandidate;
    out["isStairs"] = m_isStairs;
}

void RoomArea::Deserialize(const nlohmann::json& in)
{
    if (in.contains("minX")) m_min.x = in["minX"];
    if (in.contains("minY")) m_min.y = in["minY"];
    if (in.contains("minZ")) m_min.z = in["minZ"];
    if (in.contains("maxX")) m_max.x = in["maxX"];
    if (in.contains("maxY")) m_max.y = in["maxY"];
    if (in.contains("maxZ")) m_max.z = in["maxZ"];
    if (in.contains("isCandidate")) m_isGhostRoomCandidate = in["isCandidate"];
    if (in.contains("isStairs")) m_isStairs = in["isStairs"];
}

void RoomArea::PreDraw()
{
    // AABB��8���_���Z�o
    Math::Vector3 c[8] = {
        { m_min.x, m_min.y, m_min.z },
        { m_max.x, m_min.y, m_min.z },
        { m_min.x, m_max.y, m_min.z },
        { m_max.x, m_max.y, m_min.z },
        { m_min.x, m_min.y, m_max.z },
        { m_max.x, m_min.y, m_max.z },
        { m_min.x, m_max.y, m_max.z },
        { m_max.x, m_max.y, m_max.z },
    };

    // �S�[�X�g�̕����Ȃ�A�ʏ�͔������O���[
    ImU32 color = IM_COL32(150, 150, 150, 100);

    auto& ecs = GameManager::Instance().GetECS();
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            if (g->GetTargetRoom() == this) {
                color = IM_COL32(100, 200, 255, 255); // �S�[�X�g�̕����͐ŋ���
            }
            break;
        }
    }

    auto& cm = CollisionManager::Instance();

    // ��� (Y=min) ��4��
    cm.AddDebugLine(c[0], c[1], color);
    cm.AddDebugLine(c[1], c[3], color);
    cm.AddDebugLine(c[3], c[2], color);
    cm.AddDebugLine(c[2], c[0], color);

    // �V�� (Y=max) ��4��
    cm.AddDebugLine(c[4], c[5], color);
    cm.AddDebugLine(c[5], c[7], color);
    cm.AddDebugLine(c[7], c[6], color);
    cm.AddDebugLine(c[6], c[4], color);

    // �c4��
    cm.AddDebugLine(c[0], c[4], color);
    cm.AddDebugLine(c[1], c[5], color);
    cm.AddDebugLine(c[2], c[6], color);
    cm.AddDebugLine(c[3], c[7], color);
}

void RoomArea::ImGuiUpdate()
{
    ImGui::DragFloat3("Min", &m_min.x, 0.1f);
    ImGui::DragFloat3("Max", &m_max.x, 0.1f);
    ImGui::Checkbox("Ghost Room Candidate", &m_isGhostRoomCandidate);
    ImGui::Checkbox("Is Stairs", &m_isStairs);
}

Math::Vector3 RoomArea::GetCenter() const
{
    return (m_min + m_max) * 0.5f;
}

bool RoomArea::IsInside(const Math::Vector3& position) const
{
    return
        position.x >= m_min.x &&
        position.x <= m_max.x &&
        position.y >= m_min.y &&
        position.y <= m_max.y &&
        position.z >= m_min.z &&
        position.z <= m_max.z;
}
