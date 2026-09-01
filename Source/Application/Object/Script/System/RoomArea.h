#pragma once

class RoomArea : public NativeScript {
public:
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void PreDraw() override;
    void ImGuiUpdate() override;

    bool IsInside(const Math::Vector3& position) const;
    Math::Vector3 GetCenter() const;

    bool m_isGhostRoomCandidate = false;

    // Marks this RoomArea as a staircase rather than a normal flat room: its own min/max Y span
    // is treated as "floor level at the bottom" to "floor level at the top" instead of "floor" to
    // "ceiling", and BuildManualNavMesh bridges those two levels directly instead of applying the
    // usual same-floor adjacency check (which would otherwise reject it as touching nothing, since
    // a staircase's whole point is to span more height than kSameFloorTolerance allows).
    bool m_isStairs = false;

    Math::Vector3 m_min = { -5.0f, -5.0f, -5.0f };
    Math::Vector3 m_max = {  5.0f,  5.0f,  5.0f };
};
