#include "../../../Pch.h"
#include "TitleScene.h"
#include "../../../Framework/Manager/Scene/SceneManager.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../GameScene/GameScene.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Graphics/Renderer/Renderer.h"

void TitleScene::Init()
{
}

void TitleScene::Update(float deltaTime)
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Enter) || 
        Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Space))
    {
        SceneManager::Instance().ChangeScene(std::make_unique<GameScene>(), 1.0f);
    }

    Renderer::BeginFrame();
    GraphicsDevice::Instance().SetBackBuffer();
    GraphicsDevice::Instance().ClearBackBuffer(0.1f, 0.1f, 0.2f, 1.0f);


    Editor::Draw();

    Renderer::EndFrame();
}

