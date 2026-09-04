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

    // �G�f�B�^��ŋ��͈̔͂������f�o�b�O�`��p
    float m_debugSize = 2.0f;

    // �Ή�����RoomArea(GameObject��)�B��Ȃ��ɃA�N�e�B�u(�]���ʂ�̋���)�B
    // �ݒ肷��ƃv���C���[������RoomArea���ɂ��鎞�������˂�L��������B
    std::string m_roomName; // unused (kept for old scene data load compatibility)

    // Reflection is active whenever the player is within this distance of the mirror plane,
    // instead of the old "player is inside this named RoomArea" check - a window sitting right at
    // a room's boundary line meant standing normally in front of it never counted as "inside",
    // so the reflection never activated at realistic viewing distance.
    float m_activationDistance = 4.0f;

private:
    bool m_isActive = true;
};
