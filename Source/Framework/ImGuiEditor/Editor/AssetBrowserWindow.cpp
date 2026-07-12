#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"

static int s_selectedModelType = 0;

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}

void Editor::DrawAssetBrowser() 
{
    ImGui::Begin("Asset Browser");

    if (!s_selectedObject)
    {
        ImGui::TextDisabled("No object selected in Hierarchy.");
        ImGui::End();
        return;
    }

    bool hasModel = GameManager::Instance().GetECS().TryGetComponent<ModelRenderData>(s_selectedObject->GetEntityID()) != nullptr;
    bool hasSprite = GameManager::Instance().GetECS().TryGetComponent<SpriteData>(s_selectedObject->GetEntityID()) != nullptr;
    if (!hasModel && !hasSprite) {
        ImGui::TextDisabled("Selected object needs ModelRenderData or SpriteData.");
        ImGui::End();
        return;
    }

    // Initialize root asset dir if empty
    if (s_currentAssetDir.empty()) {
        s_currentAssetDir = "Asset";
    }

    // Back button
    if (s_currentAssetDir != "Asset" && s_currentAssetDir != "Asset\\" && s_currentAssetDir != "Asset/") {
        if (ImGui::Button("Back (..)")) {
            s_currentAssetDir = std::filesystem::path(s_currentAssetDir).parent_path().string();
            if (s_currentAssetDir.empty() || s_currentAssetDir == ".") s_currentAssetDir = "Asset";
        }
        ImGui::Separator();
    }

    // Ensure directory exists
    if (!std::filesystem::exists(s_currentAssetDir)) {
        std::filesystem::create_directories(s_currentAssetDir);
    }

    // Directory contents
    for (const auto& entry : std::filesystem::directory_iterator(s_currentAssetDir)) {
        std::string filename = entry.path().filename().string();
        if (entry.is_directory()) {
            if (ImGui::Selectable(("[Dir] " + filename).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    s_currentAssetDir = entry.path().string();
                }
            }
        } else {
            std::string pathStr = entry.path().string();
            std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
            bool isSelected = (s_selectedAssetPath == pathStr);
            if (ImGui::Selectable(filename.c_str(), isSelected)) {
                s_selectedAssetPath = pathStr;
            }
        }
    }

    ImGui::Separator();

    ImGui::Text("Selected Asset: %s", s_selectedAssetPath.c_str());

    if (hasModel) {
        ImGui::Text("Model Type:");
        ImGui::RadioButton("Static", &s_selectedModelType, 0); ImGui::SameLine();
        ImGui::RadioButton("Dynamic", &s_selectedModelType, 1);
        if (ImGui::Button("Apply to ModelRenderData")) {
            if (!s_selectedAssetPath.empty()) {
                auto& data = GameManager::Instance().GetECS().GetComponent<ModelRenderData>(s_selectedObject->GetEntityID());
                data.m_modelType = (ModelType)s_selectedModelType;
                data.m_spModelData = std::make_shared<ModelData>();
                data.m_spModelData->Load(s_selectedAssetPath);
                data.m_filePath = s_selectedAssetPath;
            }
        }
    }

    if (hasSprite) {
        if (ImGui::Button("Apply to SpriteData")) {
            if (!s_selectedAssetPath.empty()) {
                auto& data = GameManager::Instance().GetECS().GetComponent<SpriteData>(s_selectedObject->GetEntityID());
                data.m_filePath = s_selectedAssetPath;
                data.m_spTexture = nullptr; // Force reload
            }
        }
    }

    ImGui::End();
}
