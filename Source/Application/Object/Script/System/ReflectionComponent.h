#pragma once

#include "../../../../Framework/ECS/Components/Data/NativeScript.h"

class ReflectionComponent : public NativeScript {
public:
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;
    void Update(float deltaTime) override;
    void PreDraw() override;

    bool IsActive() const { return m_isActive; }

    Math::Vector3 m_planeNormal = { 0.0f, 0.0f, 1.0f };
    Math::Vector3 m_planePoint = { 0.0f, 0.0f, 0.0f };

    // Calculated World Plane
    Math::Vector3 m_worldPlaneNormal = { 0.0f, 0.0f, 1.0f };
    Math::Vector3 m_worldPlanePoint = { 0.0f, 0.0f, 0.0f };

    // エディタ上で鏡の範囲を示すデバッグ描画用
    float m_debugSize = 2.0f;

    // 対応するRoomArea(GameObject名)。空なら常にアクティブ(従来通りの挙動)。
    // 設定するとプレイヤーがそのRoomArea内にいる時だけ反射を有効化する。
    std::string m_roomName;

private:
    bool m_isActive = true;
};
