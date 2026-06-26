#include "Pch.h"
#include "TitleScene.h"
#include "../../../Framework/DirectX/Utility/Input.h"
#include "../../../Framework/Manager/SceneManager.h"
#include "../../../Framework/ECS/CompSystem/Systems/RenderSystem.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/DirectX/Utility/Logger.h"
#include "../GameScene/GameScene.h"
#include "Framework/ImGuiEditor/ImGui/imgui.h"

void TitleScene::Init()
{
}

void TitleScene::Update()
{
    // エンターキー、スペースキー、またはマウスクリックで遷移
    if (Input::Instance().IsKeyTrigger(VK_RETURN) || 
        Input::Instance().IsKeyTrigger(VK_SPACE) || 
        Input::Instance().IsMouseLeftTrigger())
    {
        SceneManager::Instance().ChangeScene(std::make_shared<GameScene>(), 1.0f);
    }

    // デバッグ表示用
    ImGui::Begin("Title Scene");
    ImGui::Text("Press ENTER or SPACE to start the game!");
    if (ImGui::Button("Start Game", ImVec2(200, 50)))
    {
        SceneManager::Instance().ChangeScene(std::make_shared<GameScene>(), 1.0f);
    }
    ImGui::End();

    // ログも表示する
    Logger::Instance().DrawImGuiWindow();
}
