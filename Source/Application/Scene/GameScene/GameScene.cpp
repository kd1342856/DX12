#include "../../../Graphics/Descriptors/Heap/CBVSRVUAVHeap/CBVSRVUAVHeap.h"
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
#include "../../../Framework/ECS/Components/Data/TransformData.h"
#include "../../../Framework/ECS/Components/Data/CameraData.h"
#include "../../../Framework/ECS/Components/Data/ModelRenderData.h"
#include "../../../Framework/ECS/Components/Data/ShaderData.h"

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
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Scene Loaded");

        // TEST SERIALIZATION OF SHAPES
        nlohmann::json testOut;
        m_spScene->Serialize(testOut);
        std::ofstream outTest("Asset/Data/Scene/TestSerialize.json");
        outTest << std::setw(4) << testOut << std::endl;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "TestSerialize.json Written");
    } else 
    { 
    {
        auto cameraObj = m_spScene->CreateGameObject("MainCamera");
        TransformData camTrans;
        camTrans.m_position = Math::Vector3(0, 0, -5.0f);
        cameraObj->AddComponent<TransformData>(camTrans);
        cameraObj->AddComponent<CameraData>(CameraData{});
    }
}

void GameScene::Update()
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F1)) { m_showEditor = !m_showEditor; }
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5)) { m_fullscreenGame = !m_fullscreenGame; }

    if (!Editor::GetEditorMode() || m_fullscreenGame)
    {
        UpdateInput();
    }
    
    UpdateCameras();
    m_spScene->Update();

    Render();
}

void GameScene::UpdateInput()
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Space))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Space pressed!");
    }
}

void GameScene::UpdateCameras()
{
    auto& ecs = GameManager::Instance().GetECS();
    m_editorCameraEntity = INVALID_ENTITY;
    m_gameCameraEntity = INVALID_ENTITY;
    std::shared_ptr<GameObject> pEditorCameraObj = nullptr;

    std::function<void(const std::shared_ptr<GameObject>&)> findCameras = [&](const std::shared_ptr<GameObject>& obj) {
        if (obj->HasComponent<CameraData>())
        {
            if (obj->GetName() == "MainCamera")
            {
                m_editorCameraEntity = obj->GetEntityID();
                pEditorCameraObj = obj;
                
                // 強制的にバグを取り除くための安全装置
                if (ecs.HasComponent<NativeScriptData>(obj->GetEntityID())) {
                    ecs.RemoveComponent<NativeScriptData>(obj->GetEntityID());
                }
                if (ecs.HasComponent<ColliderData>(obj->GetEntityID())) {
                    ecs.RemoveComponent<ColliderData>(obj->GetEntityID());
                }
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

    if (m_gameCameraEntity == INVALID_ENTITY) {
        m_gameCameraEntity = m_editorCameraEntity;
    }

    // Relative Mouse Mode Toggle
    if (Input::Instance().IsMouseRightTrigger() && !ImGui::GetIO().WantCaptureMouse) {
        Input::Instance().SetMouseModeRelative();
        m_isCameraDragging = true;
    } else if (Input::Instance().IsMouseRightRelease() && m_isCameraDragging) {
        Input::Instance().SetMouseModeAbsolute();
        m_isCameraDragging = false;
    }

    // Control Logic
    if (Editor::GetEditorMode() && !m_fullscreenGame)
    {
        // Editor Free Camera
        if (m_editorCameraEntity != INVALID_ENTITY && pEditorCameraObj && m_isCameraDragging)
        {
            if (pEditorCameraObj->HasComponent<TransformData>() && pEditorCameraObj->HasComponent<CameraData>())
            {
                auto& data = pEditorCameraObj->GetComponent<TransformData>();
                auto& camData = pEditorCameraObj->GetComponent<CameraData>();

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

                if (Input::Instance().IsKeyHold('W')) moveVec += forward;
                if (Input::Instance().IsKeyHold('S')) moveVec -= forward;
                if (Input::Instance().IsKeyHold('D')) moveVec += right;
                if (Input::Instance().IsKeyHold('A')) moveVec -= right;
                if (Input::Instance().IsKeyHold('E')) moveVec += up;
                if (Input::Instance().IsKeyHold('Q')) moveVec -= up;

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
            if (obj->GetEntityID() == m_gameCameraEntity) pGameCamObj = obj;
            for (const auto& child : obj->GetChildren()) findPlayer(child);
        };
        for (auto const& obj : m_spScene->GetGameObjects()) {
            findPlayer(obj);
        }

        if (pPlayerObj && pGameCamObj) {
            if (pPlayerObj->HasComponent<TransformData>() && pGameCamObj->HasComponent<TransformData>() && pGameCamObj->HasComponent<CameraData>()) {
                auto& pData = pPlayerObj->GetComponent<TransformData>();
                auto& cData = pGameCamObj->GetComponent<TransformData>();
                auto& camData = pGameCamObj->GetComponent<CameraData>();

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
    auto renderSystem = m_spScene->GetRenderSystem();
    if (!renderSystem) return;

    D3D12_VIEWPORT viewport = {};
    viewport.Width = 1280.0f;
    viewport.Height = 720.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissorRect = { 0, 0, 1280, 720 };
    GraphicsDevice::Instance().GetCmdList()->RSSetViewports(1, &viewport);
    GraphicsDevice::Instance().GetCmdList()->RSSetScissorRects(1, &scissorRect);

    if (m_fullscreenGame)
    {
        GraphicsDevice::Instance().SetBackBuffer();
        if (m_gameCameraEntity != INVALID_ENTITY)
        {
            renderSystem->Update(m_gameCameraEntity, nullptr);
        }
        else
        {
            GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else
    {
        // 1. Render to GameView RenderTarget using Game Camera
        m_upRenderTarget->TransitionToRenderTarget();
        GraphicsDevice::Instance().SetRenderTarget(m_upRenderTarget.get());
        if (m_gameCameraEntity != INVALID_ENTITY)
        {
            m_upRenderTarget->Clear();
            renderSystem->Update(m_gameCameraEntity, m_upRenderTarget.get());
            CollisionManager::Instance().DrawDebugWires(0, 0, 1280.0f, 720.0f, m_gameCameraEntity);
        }
        else
        {
            m_upRenderTarget->Clear();
        }
        m_upRenderTarget->TransitionToShaderResource();

        // 2. Render to BackBuffer using Editor Camera
        GraphicsDevice::Instance().SetBackBuffer();
        if (m_editorCameraEntity != INVALID_ENTITY)
        {
            renderSystem->Update(m_editorCameraEntity, nullptr);
            CollisionManager::Instance().DrawDebugWires(0, 0, 1280.0f, 720.0f, m_editorCameraEntity);
        }

        // 3. Draw ImGui
        if (Editor::GetEditorMode() && m_showEditor) { Editor::DrawGameView(m_upRenderTarget.get(), m_gameCameraEntity, false);
            Editor::DrawHierarchyAndInspector(m_spScene.get());
            Logger::Instance().DrawImGuiWindow();
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("GameScene");

            ImVec2 vMin = ImGui::GetWindowContentRegionMin();
            ImVec2 vMax = ImGui::GetWindowContentRegionMax();
            vMin.x += ImGui::GetWindowPos().x;
            vMin.y += ImGui::GetWindowPos().y;
            vMax.x += ImGui::GetWindowPos().x;
            vMax.y += ImGui::GetWindowPos().y;
            ImGui::Image((ImTextureID)GraphicsDevice::Instance().GetCBVSRVUAVHeap()->GetGPUHandle(m_upRenderTarget->GetImGuiSRVIndex()).ptr, ImVec2(vMax.x - vMin.x, vMax.y - vMin.y));

            if (ImGui::IsItemClicked())
            {
                ImGui::SetWindowFocus("GameScene");
            }
            bool isWindowFocused = ImGui::IsWindowFocused();

            ImGui::End();
            ImGui::PopStyleVar();

            if (isWindowFocused)
            {
                ImGui::GetIO().WantCaptureMouse = false;
                ImGui::GetIO().WantCaptureKeyboard = false;
            }
            else
            {
                ImGui::GetIO().WantCaptureMouse = true;
                ImGui::GetIO().WantCaptureKeyboard = true;
            }
        }
    }
}






