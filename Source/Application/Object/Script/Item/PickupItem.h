#pragma once
#include "ItemTypes.h"

// A world pickup item placed in the field.
// No longer auto-collected on touch - the Player scans for the nearest one in range/in view each
// frame (see Player::UpdateInteractionTarget) and calls Collect() when the player presses F.
class PickupItem : public NativeScript {
public:
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;

    ItemType m_itemType = ItemType::Thermometer;
    bool m_collected = false;

    // Called by Player when the player presses F while this item is the current interaction target.
    // Grants the item to the player and removes this pickup from the field.
    void Collect(class Player* player);
};
