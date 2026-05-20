#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"
#include "TransformComponent.h"

// =============================================
// CameraComponent
// カメラのView/Proj行列を管理するコンポーネント
// データはECS側のCameraDataに保持される
// =============================================
class CameraComponent : public ComponentBase {
public:
    // AddComponent<T> の自動ECS登録に使われるデータ型
    using DataType = CameraData;

    const char* GetComponentName() const override { return "CameraComponent"; }

    // デシリアライズ経由(非テンプレートAddComponent)でのECS登録
    void RegisterECSData() override {
        CameraData data{};
        GameManager::Instance().GetECS().AddComponent(GetGameObject()->GetEntityID(), data);
    }

    void ImGuiUpdate() override {
        auto& data = GetData();
        ImGui::DragFloat("FOV", &data.m_fov, 0.5f, 1.0f, 179.0f);
        ImGui::DragFloat("NearZ", &data.m_nearZ, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("FarZ", &data.m_farZ, 1.0f, 10.0f, 10000.0f);
    }

    // Awake では ECS 登録は AddComponent が自動でやる
    // カメラエンティティとして RenderSystem に登録だけ行う
    void Awake() override {
        GetGameObject()->GetScene()->GetRenderSystem()->SetCameraEntity(GetGameObject()->GetEntityID());
    }

    void Update() override {
        auto& data = GetData();
        auto pSpTransform = GetGameObject()->GetComponent<TransformComponent>();
        if (pSpTransform) {
            Math::Vector3 pos = pSpTransform->GetData().m_position;
            Math::Vector3 rot = pSpTransform->GetData().m_rotation;

            // カメラの回転行列を作成（Yaw, Pitch, Roll）
            Math::Matrix mRot = Math::Matrix::CreateFromYawPitchRoll(rot.y, rot.x, rot.z);

            // View行列 = 平行移動の逆 × 回転の逆（転置）
            Math::Matrix mTrans = Math::Matrix::CreateTranslation(-pos);
            data.m_viewMatrix = mTrans * mRot.Transpose();
        }

        data.m_projMatrix = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(data.m_fov),
            1280.0f / 720.0f,
            data.m_nearZ,
            data.m_farZ
        );
    }

    void Serialize(nlohmann::json& out) const override {
        auto& data = const_cast<CameraComponent*>(this)->GetData();
        out["Fov"] = data.m_fov;
        out["NearZ"] = data.m_nearZ;
        out["FarZ"] = data.m_farZ;
    }

    void Deserialize(const nlohmann::json& in) override {
        auto& data = GetData();
        if (in.contains("Fov")) data.m_fov = in["Fov"];
        if (in.contains("NearZ")) data.m_nearZ = in["NearZ"];
        if (in.contains("FarZ")) data.m_farZ = in["FarZ"];
    }

    // ECS側のCameraDataへの直接アクセサ（GameManager経由）
    CameraData& GetData() {
        return GameManager::Instance().GetECS().GetComponent<CameraData>(GetGameObject()->GetEntityID());
    }
};

REGISTER_COMPONENT(CameraComponent);