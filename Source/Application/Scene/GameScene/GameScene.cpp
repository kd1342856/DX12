#include "Pch.h"
#include "GameScene.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Framework/DirectX/Utility/Input.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/DirectX/Utility/Logger.h"
#include "../../../Framework/DirectX/Utility/Time.h"
#include "../../../Framework/Manager/CollisionManager.h"
#include <fstream>
#include <functional>
#include "../../../Framework/ECS/Components/TransformComponent.h"
#include "../../../Framework/ECS/Components/CameraComponent.h"
#include "../../../Framework/ECS/Components/ModelRendererComponent.h"
#include "../../../Framework/ECS/Components/ScriptComponent.h"
#include "../../../Framework/ECS/Components/PostProcessComponent.h"

void GameScene::Init()
{
    auto pDevice = GraphicsDevice::Instance().GetDevice();
    m_upRenderTarget = std::make_unique<RenderTarget>();
    m_upRenderTarget->Create(1280, 720);

    m_spScene = std::make_shared<Scene>();
    GameManager::Instance().Init();
    m_spScene->Init();

    std::ifstream in("Asset/Data/Scene/GameScene.json");
    if (in.is_open())
    {
        nlohmann::json j;
        in >> j;
        m_spScene->Deserialize(j);
        Logger::Instance().AddLog(Logger::LogLevel::Info, "シーンのロード完了");
    } else 
    {
        auto cameraObj = m_spScene->CreateGameObject("MainCamera");
        auto pCamT = cameraObj->AddComponent<TransformComponent>();
        pCamT->GetData().m_position = Math::Vector3(0, 0, -5.0f);
        cameraObj->AddComponent<CameraComponent>();
        cameraObj->AddComponent<PostProcessComponent>();
    }
}

void GameScene::Update()
{
    // Toggle game mode
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1) || Input::Instance().IsKeyTrigger(VK_F1))
    {
        m_fullscreenGame = !m_fullscreenGame;
    }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5) || Input::Instance().IsKeyTrigger(VK_F5))
    {
        m_fullscreenGame = true;
    }

    UpdateInput();
    
    m_spScene->Update();

    UpdateCameras();
    
    Render();
}

void GameScene::UpdateInput()
{
    // Relative Mouse Mode Toggle
    if (Input::Instance().IsMouseRightTrigger() && !ImGui::GetIO().WantCaptureMouse) {
        Input::Instance().SetMouseModeRelative();
        m_isCameraDragging = true;
    } else if (Input::Instance().IsMouseRightRelease() && m_isCameraDragging) {
        Input::Instance().SetMouseModeAbsolute();
        m_isCameraDragging = false;
    }

    m_editorCameraEntity = INVALID_ENTITY;
    m_gameCameraEntity = INVALID_ENTITY;
    std::shared_ptr<GameObject> pEditorCameraObj = nullptr;

    std::function<void(const std::shared_ptr<GameObject>&)> findCameras = [&](const std::shared_ptr<GameObject>& obj) {
        if (obj->GetComponent<CameraComponent>())
        {
            if (obj->GetName() == "MainCamera")
            {
                m_editorCameraEntity = obj->GetEntityID();
                pEditorCameraObj = obj;
            }
            else
            {
                m_gameCameraEntity = obj->GetEntityID();
            }
        }
        for (const auto& child : obj->GetChildren())
        {
            findCameras(child);
        }
    };

    for (auto const& obj : m_spScene->GetGameObjects())
    {
        findCameras(obj);
    }

    // Fallback if no game camera
    if (m_gameCameraEntity == INVALID_ENTITY) {
        m_gameCameraEntity = m_editorCameraEntity;
    }

    // Control Logic
    if (Editor::GetEditorMode() && !m_fullscreenGame)
    {
        // Editor Free Camera
        if (m_editorCameraEntity != INVALID_ENTITY && pEditorCameraObj && m_isCameraDragging)
        {
            auto pTrans = pEditorCameraObj->GetComponent<TransformComponent>();
            auto pCamComp = pEditorCameraObj->GetComponent<CameraComponent>();
            if (pTrans && pCamComp)
            {
                auto& data = pTrans->GetData();
                auto& camData = pCamComp->GetData();

                float rotSpeed = 0.002f;
                data.m_rotation.y += Input::Instance().GetMouseDeltaX() * rotSpeed;
                data.m_rotation.x += Input::Instance().GetMouseDeltaY() * rotSpeed;

                float pitchLimit = DirectX::XMConvertToRadians(89.0f);
                if (data.m_rotation.x > pitchLimit) data.m_rotation.x = pitchLimit;
                if (data.m_rotation.x < -pitchLimit) data.m_rotation.x = -pitchLimit;

                Math::Matrix mRot = Math::Matrix::CreateFromYawPitchRoll(data.m_rotation.y, data.m_rotation.x, data.m_rotation.z);
                Math::Vector3 forward = Math::Vector3::TransformNormal(Math::Vector3(0, 0, 1), mRot);
                Math::Vector3 right = Math::Vector3::TransformNormal(Math::Vector3(1, 0, 0), mRot);
                Math::Vector3 up = Math::Vector3(0, 1, 0);

                Math::Vector3 moveVec = Math::Vector3(0, 0, 0);
                float moveSpeed = camData.m_moveSpeed;

                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::W)) moveVec += forward;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::S)) moveVec -= forward;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::D)) moveVec += right;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::A)) moveVec -= right;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::E)) moveVec += up;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::Q)) moveVec -= up;

                if (moveVec.LengthSquared() > 0.0f)
                {
                    moveVec.Normalize();
                    data.m_position += moveVec * moveSpeed;
                }
            }
        }
    }
}

void GameScene::UpdateCameras()
{
    // Player Control Mode
    if (!Editor::GetEditorMode() || m_fullscreenGame)
    {
        std::shared_ptr<GameObject> pPlayerObj = nullptr;
        std::shared_ptr<GameObject> pGameCamObj = nullptr;

        std::function<void(const std::shared_ptr<GameObject>&)> findPlayer = [&](const std::shared_ptr<GameObject>& obj) {
            if (obj->GetName() == "Player") pPlayerObj = obj;
            if (obj->GetEntityID() == m_gameCameraEntity) pGameCamObj = obj;
            for (const auto& child : obj->GetChildren()) findPlayer(child);
        };
        for (auto const& obj : m_spScene->GetGameObjects()) {
            findPlayer(obj);
        }

        if (pPlayerObj && pGameCamObj) {
            auto pPlayerTrans = pPlayerObj->GetComponent<TransformComponent>();
            auto pCamTrans = pGameCamObj->GetComponent<TransformComponent>();
            auto pCamComp = pGameCamObj->GetComponent<CameraComponent>();
            
            if (pPlayerTrans && pCamTrans && pCamComp) {
                auto& pData = pPlayerTrans->GetData();
                auto& cData = pCamTrans->GetData();
                auto& camData = pCamComp->GetData();

                if (m_isCameraDragging || m_fullscreenGame) {
                    float rotSpeed = 0.002f;
                    pData.m_rotation.y += Input::Instance().GetMouseDeltaX() * rotSpeed;
                    cData.m_rotation.x += Input::Instance().GetMouseDeltaY() * rotSpeed;

                    float pitchLimit = DirectX::XMConvertToRadians(89.0f);
                    if (cData.m_rotation.x > pitchLimit) cData.m_rotation.x = pitchLimit;
                    if (cData.m_rotation.x < -pitchLimit) cData.m_rotation.x = -pitchLimit;
                }

                Math::Matrix playerRot = Math::Matrix::CreateRotationY(pData.m_rotation.y);
                Math::Vector3 forward = Math::Vector3::TransformNormal(Math::Vector3(0, 0, 1), playerRot);
                Math::Vector3 right = Math::Vector3::TransformNormal(Math::Vector3(1, 0, 0), playerRot);

                if (camData.m_cameraMode == CameraMode::FPS) {
                    cData.m_position = camData.m_fpsOffset; // Local
                    cData.m_rotation.y = 0;
                } else if (camData.m_cameraMode == CameraMode::TPS) {
                    Math::Matrix localRot = Math::Matrix::CreateRotationX(cData.m_rotation.x);
                    Math::Vector3 offset = Math::Vector3::TransformNormal(camData.m_targetOffset, localRot);
                    cData.m_position = offset; // Local
                    cData.m_rotation.y = 0;
                }
            }
        }
    }
}

void GameScene::Render()
{
    m_spScene->PreDraw();

    if (m_fullscreenGame)
    {
        GraphicsDevice::Instance().SetBackBuffer();
        if (m_gameCameraEntity != INVALID_ENTITY)
        {
            m_spScene->GetRenderSystem()->Update(m_gameCameraEntity);
        }
        else
        {
            GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        }
        m_spScene->Draw();
    }
    else
    {
        GraphicsDevice::Instance().SetResourceBarrier(
            m_upRenderTarget->GetResource(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        GraphicsDevice::Instance().SetRenderTarget(m_upRenderTarget.get());
        if (m_gameCameraEntity != INVALID_ENTITY)
        {
            m_upRenderTarget->Clear(0.2f, 0.2f, 0.3f, 1.0f);
            m_spScene->GetRenderSystem()->Update(m_gameCameraEntity, m_upRenderTarget.get());
        }
        else
        {
            m_upRenderTarget->Clear(0.0f, 0.0f, 0.0f, 1.0f);
        }

        GraphicsDevice::Instance().SetResourceBarrier(
            m_upRenderTarget->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

        GraphicsDevice::Instance().SetBackBuffer();
        m_spScene->GetRenderSystem()->Update(m_editorCameraEntity);
        CollisionManager::Instance().DrawDebugWires(0, 0, 1280.0f, 720.0f, m_editorCameraEntity);
        m_spScene->Draw();

        if (m_showEditor)
        {
            Editor::DrawGameView(m_upRenderTarget.get(), m_gameCameraEntity, false);
            Editor::DrawHierarchyAndInspector(m_spScene.get());
            Logger::Instance().DrawImGuiWindow();
        }
    }
}
