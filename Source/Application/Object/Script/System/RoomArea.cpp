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
    out["floorType"] = static_cast<int>(m_floorType);
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
    if (in.contains("floorType")) m_floorType = static_cast<FloorType>((int)in["floorType"]);
}

void RoomArea::PreDraw()
{
    // AABBの8頂点を算出
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

    // ゴーストの部屋なら水色、通常は半透明のグレー
    ImU32 color = IM_COL32(150, 150, 150, 100);

    auto& ecs = GameManager::Instance().GetECS();
    for (auto& scriptData : ecs.GetComponentArray<NativeScriptData>()) {
        if (auto* g = dynamic_cast<GhostAI*>(scriptData.Instance.get())) {
            if (g->GetTargetRoom() == this) {
                color = IM_COL32(100, 200, 255, 255); // ゴーストの部屋は青で強調
            }
            break;
        }
    }

    auto& cm = CollisionManager::Instance();

    // 底面 (Y=min) の4辺
    cm.AddDebugLine(c[0], c[1], color);
    cm.AddDebugLine(c[1], c[3], color);
    cm.AddDebugLine(c[3], c[2], color);
    cm.AddDebugLine(c[2], c[0], color);

    // 天井 (Y=max) の4辺
    cm.AddDebugLine(c[4], c[5], color);
    cm.AddDebugLine(c[5], c[7], color);
    cm.AddDebugLine(c[7], c[6], color);
    cm.AddDebugLine(c[6], c[4], color);

    // 縦4辺
    cm.AddDebugLine(c[0], c[4], color);
    cm.AddDebugLine(c[1], c[5], color);
    cm.AddDebugLine(c[2], c[6], color);
    cm.AddDebugLine(c[3], c[7], color);
}

void RoomArea::ImGuiUpdate()
{
    ImGui::DragFloat3(u8"最小", &m_min.x, 0.1f);
    ImGui::DragFloat3(u8"最大", &m_max.x, 0.1f);
    ImGui::Checkbox(u8"ゴーストの部屋の候補", &m_isGhostRoomCandidate);
    ImGui::Checkbox(u8"階段", &m_isStairs);
    int floor = static_cast<int>(m_floorType);
    const char* floorNames[] = { u8"木の床", u8"畳", u8"土" };
    if (ImGui::Combo(u8"床の種類", &floor, floorNames, IM_ARRAYSIZE(floorNames))) {
        m_floorType = static_cast<FloorType>(floor);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"足音の種類。どの部屋にも入っていない場所(家の外)は土になる");
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
