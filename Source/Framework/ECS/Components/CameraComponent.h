#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"
#include "TransformComponent.h"

// =============================================
// CameraComponent
// カメラのView/Proj行?Eを管?E??るコンポ?EネンチE// チE?EタはECS側のCameraDataに保持されめE// =============================================
class CameraComponent : public ComponentBase {
public:
    // AddComponent<T> の自動ECS登録に使われるデータ?E    using DataType = CameraData;

    const char* GetComponentName() const override { return "CameraComponent"; }

    // チE??リアライズ経由(非テンプレーチEddComponent)でのECS登録
    void RegisterECSData() override {
        CameraData data{};
        GameManager::Instance().GetECS().AddComponent(GetGameObject()->GetEntityID(), data);
    }

    void ImGuiUpdate() override {
        auto& data = GetData();
        ImGui::DragFloat("FOV", &data.m_fov, 0.5f, 1.0f, 179.0f);
        ImGui::DragFloat("NearZ", &data.m_nearZ, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("FarZ", &data.m_farZ, 1.0f, 10.0f, 10000.0f);
        
        // MainCameraの場合?Eみ、個別に移動速度を表示して変更可能にする
        if (GetGameObject()->GetName() == "MainCamera") {
            ImGui::DragFloat("MoveSpeed", &data.m_moveSpeed, 0.01f, 0.001f, 10.0f);
        }

        ImGui::DragFloat3("TPS Offset", &data.m_targetOffset.x, 0.1f);
        ImGui::DragFloat3("FPS Offset", &data.m_fpsOffset.x, 0.1f);

        const char* modeNames[] = { "EditorFree", "TPS", "FPS" };
        int modeIdx = static_cast<int>(data.m_cameraMode);
        if (ImGui::Combo("Camera Mode", &modeIdx, modeNames, 3)) {
            data.m_cameraMode = static_cast<CameraMode>(modeIdx);
        }
    }

    // Awake では ECS 登録は AddComponent が?E動でめE??
    // カメラエンチE??チE??として RenderSystem に登録だけ行う
    void Awake() override {
        GetGameObject()->GetScene()->GetRenderSystem()->SetCameraEntity(GetGameObject()->GetEntityID());
    }

    void Update() override {
        auto& data = GetData();
        auto pSpTransform = GetGameObject()->GetComponent<TransformComponent>();
        if (pSpTransform) {
            // 親子関係が反映されたワールド行?Eの送E???EをView行?Eとする
            Math::Matrix worldMat = pSpTransform->GetData().m_worldMatrix;
            data.m_viewMatrix = worldMat.Invert();
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
        out["MoveSpeed"] = data.m_moveSpeed;
        out["CameraMode"] = static_cast<int>(data.m_cameraMode);
        out["TargetOffsetX"] = data.m_targetOffset.x;
        out["TargetOffsetY"] = data.m_targetOffset.y;
        out["TargetOffsetZ"] = data.m_targetOffset.z;
        out["FpsOffsetX"] = data.m_fpsOffset.x;
        out["FpsOffsetY"] = data.m_fpsOffset.y;
        out["FpsOffsetZ"] = data.m_fpsOffset.z;
    }

    void Deserialize(const nlohmann::json& in) override {
        auto& data = GetData();
        if (in.contains("Fov")) data.m_fov = in["Fov"];
        if (in.contains("NearZ")) data.m_nearZ = in["NearZ"];
        if (in.contains("FarZ")) data.m_farZ = in["FarZ"];
        if (in.contains("MoveSpeed")) data.m_moveSpeed = in["MoveSpeed"];
        if (in.contains("CameraMode")) data.m_cameraMode = static_cast<CameraMode>(in["CameraMode"]);
        if (in.contains("TargetOffsetX")) data.m_targetOffset.x = in["TargetOffsetX"];
        if (in.contains("TargetOffsetY")) data.m_targetOffset.y = in["TargetOffsetY"];
        if (in.contains("TargetOffsetZ")) data.m_targetOffset.z = in["TargetOffsetZ"];
        if (in.contains("FpsOffsetX")) data.m_fpsOffset.x = in["FpsOffsetX"];
        if (in.contains("FpsOffsetY")) data.m_fpsOffset.y = in["FpsOffsetY"];
        if (in.contains("FpsOffsetZ")) data.m_fpsOffset.z = in["FpsOffsetZ"];
    }

    // Get CameraData from ECS
    CameraData& GetData() {
        return GameManager::Instance().GetECS().GetComponent<CameraData>(GetGameObject()->GetEntityID());
    }
};

REGISTER_COMPONENT(CameraComponent);