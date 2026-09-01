#include "../../../../Pch.h"
#include "PickupItem.h"
#include "../../../../Framework/Object/GameObject.h"
#include "../../../../Framework/DirectX/Utility/Logger.h"
#include "../Player/Player.h"

REGISTER_COMPONENT(PickupItem);

void PickupItem::Serialize(nlohmann::json& out) const
{
    out["ItemType"] = static_cast<int>(m_itemType);
}

void PickupItem::Deserialize(const nlohmann::json& in)
{
    if (in.contains("ItemType")) m_itemType = static_cast<ItemType>((int)in["ItemType"]);
}

void PickupItem::ImGuiUpdate()
{
    int current = static_cast<int>(m_itemType) - 1; // enum starts at 1
    const char* names[] = { "Thermometer", "Incense (SmudgeStick)", "Amulet (Ofuda)" };
    if (ImGui::Combo("Item Type", &current, names, IM_ARRAYSIZE(names))) {
        m_itemType = static_cast<ItemType>(current + 1);
    }
    ImGui::Text("[Debug] Collected: %s", m_collected ? "true" : "false");
}

void PickupItem::Collect(Player* player)
{
    if (m_collected || !player) return;

    player->AddItem(m_itemType);

    m_collected = true;
    Logger::Instance().AddLog(Logger::LogLevel::Info, "[PickupItem] Item collected via F key.");
    if (m_pGameObject) {
        m_pGameObject->Destroy();
    }
}
