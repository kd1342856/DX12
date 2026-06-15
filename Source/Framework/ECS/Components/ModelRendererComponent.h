#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"
#include "../../Manager/ResourceManager.h"

// =============================================
// ModelRendererComponent
// モデル描画データを管理するコンポーネント
// データはECS側のModelRenderDataに保持される
// =============================================
class ModelRendererComponent : public ComponentBase {
public:
    // AddComponent<T> の自動ECS登録に使われるデータ型
    using DataType = ModelRenderData;

    const char* GetComponentName() const override { return "ModelRendererComponent"; }

    // デシリアライズ経由(非テンプレートAddComponent)でのECS登録
    void RegisterECSData() override {
        ModelRenderData data{};
        GameManager::Instance().GetECS().AddComponent(GetGameObject()->GetEntityID(), data);
    }

    void ImGuiUpdate() override {
        auto& data = GetData();
        ImGui::Text("File Path: %s", data.m_filePath.empty() ? "No Model" : data.m_filePath.c_str());
        ImGui::Text("Model Type: %s", (int)data.m_modelType == 0 ? "Static" : "Dynamic");
    }

    // Awake は不要（AddComponent が自動でECS登録済み）
    void Awake() override {}

    void Serialize(nlohmann::json& out) const override {
        auto& data = const_cast<ModelRendererComponent*>(this)->GetData();
        out["FilePath"] = data.m_filePath;
        out["ModelType"] = (int)data.m_modelType;
    }

    void Deserialize(const nlohmann::json& in) override {
        auto& data = GetData();
        if (in.contains("ModelType")) {
            data.m_modelType = (ModelType)in["ModelType"];
        }
        if (in.contains("FilePath")) {
            std::string path = in["FilePath"];
            if (!path.empty()) {
                data.m_spModelData = std::make_shared<ModelData>();
                data.m_spModelData->Load(path);
                data.m_filePath = path;
            }
        }
    }

    // ECS側のModelRenderDataへの直接アクセサ（GameManager経由）
    ModelRenderData& GetData() {
        return GameManager::Instance().GetECS().GetComponent<ModelRenderData>(GetGameObject()->GetEntityID());
    }
};

REGISTER_COMPONENT(ModelRendererComponent);