#include "GameScene.h"
#include <fstream>
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Framework/DirectX/Utility/Input.h"
#include "../../../Framework/ECS/Components/TransformComponent.h"
#include "../../../Framework/ECS/Components/CameraComponent.h"
#include "../../../Framework/ECS/Components/ModelRendererComponent.h"
#include "../../../Framework/ECS/Components/ScriptComponent.h"
#include "../../../Framework/ECS/Components/PostProcessComponent.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"

void GameScene::Init()
{
    m_upRenderTarget = std::make_unique<RenderTarget>();
    m_upRenderTarget->Create(1280, 720);

    GameManager::Instance().Init();

    m_spScene = std::make_shared<Scene>();
    m_spScene->Init();

    std::ifstream in("Asset/Data/Scene/GameScene.json");
    if (in.is_open()) {
        nlohmann::json j;
        in >> j;
        m_spScene->Deserialize(j);
    } else 
    {
        auto cameraObj = m_spScene->CreateGameObject("MainCamera");
        auto pCamT = cameraObj->AddComponent<TransformComponent>();
        pCamT->GetData().m_position = Math::Vector3(0, 0, -5.0f);
        cameraObj->AddComponent<CameraComponent>();
        cameraObj->AddComponent<PostProcessComponent>(); // Add PostProcessComponent to Camera
    }
}

void GameScene::Update()
{
    // キー入力によるトグル切り替え
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1))
    {
        m_showEditor = !m_showEditor;
    }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5))
    {
        m_fullscreenGame = !m_fullscreenGame;
    }

    // メインビューポートにDockSpaceを作成し、背景を透過にする（全画面でない場合のみ）
    if (!m_fullscreenGame)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::PopStyleColor();
    }

    // Scene Update
    m_spScene->Update();

    // カメラEntityの検索（MainCameraとそれ以外）
    Entity editorCameraEntity = INVALID_ENTITY;
    Entity gameCameraEntity = INVALID_ENTITY;
    for (auto const& obj : m_spScene->GetGameObjects())
    {
        if (obj->GetComponent<CameraComponent>())
        {
            if (obj->GetName() == "MainCamera")
            {
                editorCameraEntity = obj->GetEntityID();
            }
            else
            {
                gameCameraEntity = obj->GetEntityID();
            }
        }
    }

    if (m_fullscreenGame)
    {
        // F5 全画面モード：バックバッファにプレイ用カメラで直接描画（ImGuiは使わない）
        GraphicsDevice::Instance().SetBackBuffer();
        if (gameCameraEntity != INVALID_ENTITY)
        {
            m_spScene->GetRenderSystem()->Update(gameCameraEntity);
        }
        else
        {
            // プレイ用カメラが無い時は真っ黒でクリアする
            GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else
    {
        // 通常モード：2パス描画
        // 1. Render to RenderTarget (GameView用、プレイ用カメラで描画)
        GraphicsDevice::Instance().SetRenderTarget(m_upRenderTarget.get());
        if (gameCameraEntity != INVALID_ENTITY)
        {
            m_upRenderTarget->Clear(0.2f, 0.2f, 0.3f, 1.0f);
            m_spScene->GetRenderSystem()->Update(gameCameraEntity);
        }
        else
        {
            // プレイ用カメラが設定されていない時は真っ黒でクリアする
            m_upRenderTarget->Clear(0.0f, 0.0f, 0.0f, 1.0f);
        }

        GraphicsDevice::Instance().SetResourceBarrier(
            m_upRenderTarget->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

        // 2. Render to BackBuffer (エディタ背景用、エディタ用カメラで直接描画)
        GraphicsDevice::Instance().SetBackBuffer();
        m_spScene->GetRenderSystem()->Update(editorCameraEntity);

        // エディタUIとGameViewは m_showEditor がオンのときだけ描画する
        if (m_showEditor)
        {
            Editor::DrawGameView(m_upRenderTarget.get(), false);
            // Editor UI
            Editor::DrawHierarchyAndInspector(m_spScene.get());
        }
    }
}