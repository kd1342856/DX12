#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../../../Application/Scene/GameScene/GameScene.h"
#include "../../../Application/Scene/TitleScene/TitleScene.h"

// Resolve the GetCurrentScene issue
static std::shared_ptr<Scene> GetCurrentScenePtr() {
    return Editor::GetScene();
}


void Editor::DrawDockSpace() {
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }
}

void Editor::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                auto sceneObj = GetCurrentScenePtr();
                if (sceneObj) {
                    nlohmann::json j;
                    sceneObj->Serialize(j);
                    std::ofstream o("Asset/Data/Scene/GameScene.json");
                    if (o.is_open()) {
                        o << std::setw(4) << j << std::endl;
                        Logger::Instance().AddLog(Logger::LogLevel::Info, "Scene Saved.");
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void Editor::DrawToolbar() {
    ImGui::Begin("Editor Control");

    const char* scenes[] = { "Title Scene", "Game Scene" };
    
    int current_scene = 1; // Default to GameScene
    auto currentBase = SceneManager::Instance().GetCurrentScene();
    if (dynamic_cast<TitleScene*>(currentBase)) {
        current_scene = 0;
    } else if (dynamic_cast<GameScene*>(currentBase)) {
        current_scene = 1;
    }

    if (ImGui::Combo("Scene", &current_scene, scenes, IM_ARRAYSIZE(scenes))) {
        if (current_scene == 0) {
            SceneManager::Instance().ChangeScene(std::make_unique<TitleScene>(), 0.5f);
        } else {
            SceneManager::Instance().ChangeScene(std::make_unique<GameScene>(), 0.5f);
        }
    }

    ImGui::Separator();

    ImGui::Checkbox("Editor Mode", &s_editorMode);
    bool debugWire = CollisionManager::Instance().IsDebugWireEnabled();
    if (ImGui::Checkbox("Debug Wire", &debugWire)) {
        CollisionManager::Instance().SetDebugWireEnabled(debugWire);
    }

    ImGui::End();
}

