#include "../../../Pch.h"
#include "../../Manager/Scene/Scene.h"
#include "../../Manager/Scene/SceneManager.h"
#include "../../Manager/Collision/CollisionManager.h"
#include "../../DirectX/Utility/Profiler.h"
#include "../../System/JobSystem/JobSystem.h"
#include "../../../Application/Scene/GameScene/GameScene.h"
#include "../../../Application/Scene/TitleScene/TitleScene.h"
#include "../../../Application/Scene/ResultScene/ResultScene.h"

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
                const std::string& path = GetScenePath();
                if (sceneObj && !path.empty()) {
                    nlohmann::json j;
                    sceneObj->Serialize(j);
                    std::ofstream o(path);
                    if (o.is_open()) {
                        o << std::setw(4) << j << std::endl;
                        Logger::Instance().AddLog(Logger::LogLevel::Info, "Scene Saved: %s", path.c_str());
                    }
                    SaveEditorData();
                } else {
                    Logger::Instance().AddLog(Logger::LogLevel::Warning, "このシーンには保存先がありません(保存しませんでした)");
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Game Editor",   nullptr, &s_showGameEditor);
            ImGui::MenuItem("Shader Editor", nullptr, &s_showShaderEditor);
            ImGui::MenuItem("NavMesh Editor", nullptr, &s_showNavMeshEditor);
            ImGui::MenuItem("Preview Camera", nullptr, &s_showPreviewCamera);
            ImGui::MenuItem("Sound Manager", nullptr, &s_showSoundManager);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void Editor::DrawToolbar() {
    ImGui::Begin("Editor Control");

    const char* scenes[] = { "Title Scene", "Game Scene", "Result Scene (Clear)", "Result Scene (GameOver)" };
    
    int current_scene = 1; // Default to GameScene
    auto currentBase = SceneManager::Instance().GetCurrentScene();
    if (dynamic_cast<TitleScene*>(currentBase)) {
        current_scene = 0;
    } else if (dynamic_cast<GameScene*>(currentBase)) {
        current_scene = 1;
    } else if (dynamic_cast<ResultScene*>(currentBase)) {
        current_scene = 2;
    }

    if (ImGui::Combo("Scene", &current_scene, scenes, IM_ARRAYSIZE(scenes))) {
        if (current_scene == 0) {
            SceneManager::Instance().ChangeScene(std::make_unique<TitleScene>(), 0.5f);
        } else if (current_scene == 1) {
            SceneManager::Instance().ChangeScene(std::make_unique<GameScene>(), 0.5f);
        } else if (current_scene == 2) {
            SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(true, 245.4f, 1 /*Kageonna*/, 2), 0.5f);
        } else if (current_scene == 3) {
            SceneManager::Instance().ChangeScene(std::make_unique<ResultScene>(false), 0.5f);
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

