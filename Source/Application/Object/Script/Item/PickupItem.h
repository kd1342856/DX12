#pragma once
#include "ItemTypes.h"

// A world pickup item placed in the field.
// No longer auto-collected on touch - the Player scans for the nearest one in range/in view each
// frame (see Player::UpdateInteractionTarget) and calls Collect() when the player presses F.
class PickupItem : public NativeScript {
public:
    void Update(float deltaTime) override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;

    ItemType m_itemType = ItemType::Thermometer;
    bool m_collected = false;

    // Called by Player when the player presses F while this item is the current interaction target.
    // Grants the item to the player and removes this pickup from the field.
    void Collect(class Player* player);

    // Called by Player when it throws an item away (G key): the item flies with this initial
    // velocity under gravity, bounces off walls/the floor a little, and settles on the floor.
    // Uses simple raycasts against the "Stage" mesh rather than the collision system.
    void Launch(const Math::Vector3& velocity);

    // FlashLight pickups carry a light: it stays lit when thrown while on, and can be toggled
    // on the ground with left-click (Player). Ignored for other item types.
    void SetLightOn(bool on) { m_lightOn = on; }
    bool IsLightOn() const { return m_lightOn; }
    void ToggleLight() { m_lightOn = !m_lightOn; }
    // Ghost interference: flicker the light for a while (it stays on, only the look flickers).
    void FlickerLight(float duration) { m_lightFlicker.Start(duration); }
    bool IsLightFlickering() const { return m_lightFlicker.IsFlickering(); }

private:
    void UpdateFlight(float deltaTime);

    void SyncLight(float deltaTime);
    bool  m_lightOn = false;
    LightFlickerState m_lightFlicker;

    bool  m_isFlying = false;
    Math::Vector3 m_velocity = { 0, 0, 0 };
    float m_flightTime = 0.0f;
    float m_spinSpeed = 0.0f; // yaw spin while airborne (rad/s), just for looks
};
