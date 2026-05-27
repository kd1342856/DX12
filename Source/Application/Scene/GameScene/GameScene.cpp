#include "GameScene.h"
#include <fstream>
#include <functional>
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Framework/DirectX/Utility/Input.h"
#include "../../../Framework/ECS/Components/TransformComponent.h"
#include "../../../Framework/ECS/Components/CameraComponent.h"
#include "../../../Framework/ECS/Components/ModelRendererComponent.h"
#include "../../../Framework/ECS/Components/ScriptComponent.h"
#include "../../../Framework/ECS/Components/PostProcessComponent.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Framework/DirectX/Utility/Logger.h"
#include "../../../Framework/System/Collision/CollisionManager.h"

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
        Logger::Instance().AddLog(Logger::LogLevel::Info, "シーンをロードしました");
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
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1))
    {
        m_showEditor = !m_showEditor;
    }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5))
    {
        m_fullscreenGame = !m_fullscreenGame;
    }

    if (!m_fullscreenGame)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::PopStyleColor();
    }

    m_spScene->Update();

    Entity editorCameraEntity = INVALID_ENTITY;
    Entity gameCameraEntity = INVALID_ENTITY;
    std::shared_ptr<GameObject> pEditorCameraObj = nullptr;

    std::function<void(const std::shared_ptr<GameObject>&)> findCameras = [&](const std::shared_ptr<GameObject>& obj) {
        if (obj->GetComponent<CameraComponent>())
        {
            if (obj->GetName() == "MainCamera")
            {
                editorCameraEntity = obj->GetEntityID();
                pEditorCameraObj = obj;
            }
            else
            {
                gameCameraEntity = obj->GetEntityID();
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
    if (gameCameraEntity == INVALID_ENTITY) {
        gameCameraEntity = editorCameraEntity;
    }

    // Relative Mouse Mode Toggle
    if (Input::Instance().IsMouseRightTrigger()) {
        Input::Instance().SetMouseModeRelative();
    } else if (Input::Instance().IsMouseRightRelease()) {
        Input::Instance().SetMouseModeAbsolute();
    }

    // Control Logic
    if (Editor::GetEditorMode() && !m_fullscreenGame)
    {
        // Editor Free Camera
        if (editorCameraEntity != INVALID_ENTITY && pEditorCameraObj && Input::Instance().IsMouseRightHold())
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
    else
    {
        // Player Control Mode
        std::shared_ptr<GameObject> pPlayerObj = nullptr;
        std::shared_ptr<GameObject> pGameCamObj = nullptr;

        std::function<void(const std::shared_ptr<GameObject>&)> findPlayer = [&](const std::shared_ptr<GameObject>& obj) {
            if (obj->GetName() == "Player") pPlayerObj = obj;
            if (obj->GetEntityID() == gameCameraEntity) pGameCamObj = obj;
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

                if (Input::Instance().IsMouseRightHold() || m_fullscreenGame) {
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

                Math::Vector3 moveVec = Math::Vector3(0, 0, 0);
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::W)) moveVec += forward;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::S)) moveVec -= forward;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::D)) moveVec += right;
                if (Input::Instance().IsKeyHold(DirectX::Keyboard::Keys::A)) moveVec -= right;

                if (moveVec.LengthSquared() > 0.0f) {
                    moveVec.Normalize();
                    pData.m_position += moveVec * camData.m_moveSpeed;
                }

                if (camData.m_cameraMode == CameraMode::FPS) {
                    cData.m_position = pData.m_position + Math::Vector3(0, 1.5f, 0);
                    cData.m_rotation.y = pData.m_rotation.y;
                } else if (camData.m_cameraMode == CameraMode::TPS) {
                    Math::Matrix camRot = Math::Matrix::CreateFromYawPitchRoll(pData.m_rotation.y, cData.m_rotation.x, 0.0f);
                    Math::Vector3 offset = Math::Vector3::TransformNormal(Math::Vector3(0, 2.0f, -5.0f), camRot);
                    cData.m_position = pData.m_position + offset;
                    cData.m_rotation.y = pData.m_rotation.y;
                }
            }
        }
    }

    if (m_fullscreenGame)
    {
        GraphicsDevice::Instance().SetBackBuffer();
        if (gameCameraEntity != INVALID_ENTITY)
        {
            m_spScene->GetRenderSystem()->Update(gameCameraEntity);
            CollisionManager::Instance().DrawDebugWires();
        }
        else
        {
            GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else
    {
        GraphicsDevice::Instance().SetResourceBarrier(
            m_upRenderTarget->GetResource(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        GraphicsDevice::Instance().SetRenderTarget(m_upRenderTarget.get());
        if (gameCameraEntity != INVALID_ENTITY)
        {
            m_upRenderTarget->Clear(0.2f, 0.2f, 0.3f, 1.0f);
            m_spScene->GetRenderSystem()->Update(gameCameraEntity, m_upRenderTarget.get());
            CollisionManager::Instance().DrawDebugWires();
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
        m_spScene->GetRenderSystem()->Update(editorCameraEntity);
        CollisionManager::Instance().DrawDebugWires();

        if (m_showEditor)
        {
            Editor::DrawGameView(m_upRenderTarget.get(), false);
            Editor::DrawHierarchyAndInspector(m_spScene.get());
            Logger::Instance().DrawImGuiWindow();
        }
    }
}