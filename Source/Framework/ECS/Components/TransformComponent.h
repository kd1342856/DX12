#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"

// =============================================
// TransformComponent
// 座標・回転・スケールを管理するコンポーネント
// データはECS側のTransformDataに保持される
// =============================================
class TransformComponent : public ComponentBase {
public:
    // AddComponent<T> の自動ECS登録に使われるデータ型
    using DataType = TransformData;

    const char* GetComponentName() const override { return "TransformComponent"; }

    // デシリアライズ経由(非テンプレートAddComponent)でのECS登録
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

    // Awake は不要（AddComponent が自動でECS登録済み）
    void Awake() override {}

    void Update() override {
        auto& data = GetData();
        // ローカル行列の計算
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

    // ECS側のTransformDataへの直接アクセサ（GameManager経由）
    TransformData& GetData() {
        return GameManager::Instance().GetECS().GetComponent<TransformData>(GetGameObject()->GetEntityID());
    }
};

REGISTER_COMPONENT(TransformComponent);