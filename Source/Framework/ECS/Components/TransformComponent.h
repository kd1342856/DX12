#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"
#include "../../Manager/GameManager.h"
#include "../../Object/GameObject.h"

// =============================================
// TransformComponent
// 蠎ｧ讓吶・蝗櫁ｻ｢繝ｻ繧ｹ繧ｱ繝ｼ繝ｫ繧堤ｮ｡逅・☆繧九さ繝ｳ繝昴・繝阪Φ繝・
// 繝・・繧ｿ縺ｯECS蛛ｴ縺ｮTransformData縺ｫ菫晄戟縺輔ｌ繧・
// =============================================
class TransformComponent : public ComponentBase {
public:
    // AddComponent<T> 縺ｮ閾ｪ蜍髭CS逋ｻ骭ｲ縺ｫ菴ｿ繧上ｌ繧九ョ繝ｼ繧ｿ蝙・
    using DataType = TransformData;

    const char* GetComponentName() const override { return "TransformComponent"; }

    // 繝・す繝ｪ繧｢繝ｩ繧､繧ｺ邨檎罰(髱槭ユ繝ｳ繝励Ξ繝ｼ繝・ddComponent)縺ｧ縺ｮECS逋ｻ骭ｲ
    void RegisterECSData() override {
        TransformData data{};
        GameManager::Instance().GetECS().AddComponent(GetGameObject()->GetEntityID(), data);
    }

    void ImGuiUpdate() override {
        auto& data = GetData();
        ImGui::DragFloat3("Position", &data.m_position.x, 0.05f);
        float rotDeg[3] = {
            DirectX::XMConvertToDegrees(data.m_rotation.x),
            DirectX::XMConvertToDegrees(data.m_rotation.y),
            DirectX::XMConvertToDegrees(data.m_rotation.z)
        };
        if (ImGui::DragFloat3("Rotation (Deg)", rotDeg, 1.0f)) {
            data.m_rotation = {
                DirectX::XMConvertToRadians(rotDeg[0]),
                DirectX::XMConvertToRadians(rotDeg[1]),
                DirectX::XMConvertToRadians(rotDeg[2])
            };
        }
        ImGui::DragFloat3("Scale", &data.m_scale.x, 0.05f);
    }

    // Awake 縺ｯ荳崎ｦ・ｼ・ddComponent 縺瑚・蜍輔〒ECS逋ｻ骭ｲ貂医∩・・
    void Awake() override {}

    void Update() override { UpdateMatrix(); }
    void PostUpdate() override { UpdateMatrix(); }

    void UpdateMatrix() {
        auto& data = GetData();
        // 繝ｭ繝ｼ繧ｫ繝ｫ陦悟・縺ｮ險育ｮ・
        Math::Matrix localMatrix =
            Math::Matrix::CreateScale(data.m_scale) *
            Math::Matrix::CreateFromYawPitchRoll(data.m_rotation.y, data.m_rotation.x, data.m_rotation.z) *
            Math::Matrix::CreateTranslation(data.m_position);

        if (GetGameObject()->GetParent()) {
            if (auto pParentTransform = GetGameObject()->GetParent()->GetComponent<TransformComponent>()) {
                data.m_worldMatrix = localMatrix * pParentTransform->GetData().m_worldMatrix;
            } else {
                data.m_worldMatrix = localMatrix;
            }
        } else {
            data.m_worldMatrix = localMatrix;
        }
    }

    void Serialize(nlohmann::json& out) const override {
        auto& data = const_cast<TransformComponent*>(this)->GetData();
        out["PosX"] = data.m_position.x; out["PosY"] = data.m_position.y; out["PosZ"] = data.m_position.z;
        out["RotX"] = data.m_rotation.x; out["RotY"] = data.m_rotation.y; out["RotZ"] = data.m_rotation.z;
        out["ScaX"] = data.m_scale.x;    out["ScaY"] = data.m_scale.y;    out["ScaZ"] = data.m_scale.z;
    }

    void Deserialize(const nlohmann::json& in) override {
        auto& data = GetData();
        if (in.contains("PosX")) data.m_position.x = in["PosX"];
        if (in.contains("PosY")) data.m_position.y = in["PosY"];
        if (in.contains("PosZ")) data.m_position.z = in["PosZ"];
        if (in.contains("RotX")) data.m_rotation.x = in["RotX"];
        if (in.contains("RotY")) data.m_rotation.y = in["RotY"];
        if (in.contains("RotZ")) data.m_rotation.z = in["RotZ"];
        if (in.contains("ScaX")) data.m_scale.x = in["ScaX"];
        if (in.contains("ScaY")) data.m_scale.y = in["ScaY"];
        if (in.contains("ScaZ")) data.m_scale.z = in["ScaZ"];
    }

    // ECS蛛ｴ縺ｮTransformData縺ｸ縺ｮ逶ｴ謗･繧｢繧ｯ繧ｻ繧ｵ・・ameManager邨檎罰・・
    TransformData& GetData() {
        return GameManager::Instance().GetECS().GetComponent<TransformData>(GetGameObject()->GetEntityID());
    }
};

REGISTER_COMPONENT(TransformComponent);