#include "../../../Pch.h"
#include "GameScene.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/TransformSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/CameraSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/AnimationSystem.h"
#include "../../../Framework/Manager/Collision/CollisionManager.h"
#include "../../../Framework/System/JobSystem/JobSystem.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Graphics/Renderer/Renderer.h"

void GameScene::Init()
{
    auto pDevice = GraphicsDevice::Instance().GetDevice();

    GameManager::Instance().Init();
    Editor::Init();

    std::ifstream in("Asset/Data/Scene/GameScene.json");
    if (in.is_open())
    {
        nlohmann::json j;
        in >> j;
        Editor::GetScene()->Deserialize(j);
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Scene Loaded");

        // TEST SERIALIZATION OF SHAPES
        nlohmann::json testOut;
        Editor::GetScene()->Serialize(testOut);
        std::ofstream outTest("Asset/Data/Scene/TestSerialize.json");
        outTest << std::setw(4) << testOut << std::endl;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "TestSerialize.json Written");
    }
    else
    {
        {
            auto cameraObj = Editor::GetScene()->CreateGameObject("MainCamera");
            TransformData camTrans;
            camTrans.m_position = Math::Vector3(0, 0, -5.0f);
            cameraObj->AddComponent<TransformData>(camTrans);
            cameraObj->AddComponent<CameraData>(CameraData{});
        }
    }

    Editor::GetScene()->Init(); // ここでロードされた全スクリプトのAwakeとStartが走る
    JobSystem::Instance().Wait();
}

void GameScene::HandleModeSwitch()
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::F5)) { 
        m_fullscreenGame = !m_fullscreenGame; 
        if (m_fullscreenGame) {
            Input::Instance().SetMouseModeRelative();
        } else {
            Input::Instance().SetMouseModeAbsolute();
            m_isCameraDragging = false;
        }
    }
}

void GameScene::Update(float deltaTime)
{
    HandleModeSwitch();

    if (!Editor::GetEditorMode() || m_fullscreenGame)
    {
        UpdateInput();
    }
    
    UpdateCamera();
    GameManager::Instance().Update(deltaTime, Editor::GetScene().get());

    Render();
}

void GameScene::UpdateInput()
{
    if (Input::Instance().IsKeyTrigger(DirectX::Keyboard::Keys::Space))
    {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "Space pressed!");
    }
}

void GameScene::UpdateCamera()
{
    auto& ecs = GameManager::Instance().GetECS();
    m_editorCameraEntity = INVALID_ENTITY;
    m_gameCameraEntity = INVALID_ENTITY;
    std::shared_ptr<GameObject> pEditorCameraObj = nullptr;

    std::function<void(const std::shared_ptr<GameObject>&)> findCameras = [&](const std::shared_ptr<GameObject>& obj) {
        if (ecs.TryGetComponent<CameraData>(obj->GetEntityID()) != nullptr)
        {
            if (obj->GetName() == "MainCamera")
            {
                m_editorCameraEntity = obj->GetEntityID();
                pEditorCameraObj = obj;
                
                // スクリプトやコライダーを強制削除しないように修正（フリーカメラを動かすため）
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

    for (auto const& obj : Editor::GetScene()->GetGameObjects())
    {
        findCameras(obj);
    }

    if (m_gameCameraEntity == INVALID_ENTITY) {
        m_gameCameraEntity = m_editorCameraEntity;
    }

    // Relative Mouse Mode Toggle for Editor Free Cam
    if (!m_fullscreenGame) {
        if (Input::Instance().IsMouseRightTrigger() && !ImGui::GetIO().WantCaptureMouse) {
            Input::Instance().SetMouseModeRelative();
            m_isCameraDragging = true;
        } else if (Input::Instance().IsMouseRightRelease() && m_isCameraDragging) {
            Input::Instance().SetMouseModeAbsolute();
            m_isCameraDragging = false;
        }
    } else {
        m_isCameraDragging = true; // In game mode, camera is always controlled
    }

    // Control Logic
    if (Editor::GetEditorMode() && !m_fullscreenGame)
    {
        // Editor Free Camera
        if (m_editorCameraEntity != INVALID_ENTITY && pEditorCameraObj && m_isCameraDragging)
        {
            auto* pData = ecs.TryGetComponent<TransformData>(pEditorCameraObj->GetEntityID());
            auto* pCamData = ecs.TryGetComponent<CameraData>(pEditorCameraObj->GetEntityID());
            if (pData && pCamData)
            {
                auto& data = *pData;
                auto& camData = *pCamData;

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
        for (auto const& obj : Editor::GetScene()->GetGameObjects()) {
            findPlayer(obj);
        }

        // エディタカメラ（MainCamera）がゲームカメラとして使われていて、かつゲームプレイ中でない場合はスナップさせない
        bool shouldSnap = (m_gameCameraEntity != m_editorCameraEntity) || m_fullscreenGame;
        if (pPlayerObj && pGameCamObj && shouldSnap) {
            auto* pPData = ecs.TryGetComponent<TransformData>(pPlayerObj->GetEntityID());
            auto* pCData = ecs.TryGetComponent<TransformData>(pGameCamObj->GetEntityID());
            auto* pCamData = ecs.TryGetComponent<CameraData>(pGameCamObj->GetEntityID());
            if (pPData && pCData && pCamData) {
                auto& pData = *pPData;
                auto& cData = *pCData;
                auto& camData = *pCamData;

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

                bool isChild = (pGameCamObj->GetParent() != nullptr);
                Math::Vector3 pivotOffset(0, 1.0f, 0); // プレイヤーの胸あたりE高さ

                if (camData.m_cameraMode == CameraMode::FPS) {
                    if (isChild) {
                        cData.m_position = camData.m_fpsOffset + pivotOffset;
                        cData.m_rotation.y = 0;
                    } else {
                        Math::Vector3 offset = Math::Vector3::TransformNormal(camData.m_fpsOffset + pivotOffset, playerRot);
                        cData.m_position = pData.m_position + offset;
                        cData.m_rotation.y = pData.m_rotation.y;
                    }
                } else if (camData.m_cameraMode == CameraMode::TPS) {
                    Math::Matrix localRot = Math::Matrix::CreateRotationX(cData.m_rotation.x);
                    if (isChild) {
                        Math::Vector3 offset = Math::Vector3::TransformNormal(camData.m_targetOffset, localRot);
                        cData.m_position = pivotOffset + offset; 
                        cData.m_rotation.y = 0;
                    } else {
                        Math::Vector3 offset = Math::Vector3::TransformNormal(camData.m_targetOffset, localRot * playerRot);
                        cData.m_position = pData.m_position + pivotOffset + offset;
                        cData.m_rotation.y = pData.m_rotation.y;
                    }
                }
            }
        }
    }
}

void GameScene::Render()
{
    Renderer::BeginFrame();

    auto renderSystem = GameManager::Instance().GetRenderSystem();
    if (renderSystem) {
        renderSystem->RenderShadow();
    }

    if (m_fullscreenGame)
        RenderGame();
    else
        RenderEditor();

    Renderer::EndFrame();
}

void GameScene::RenderGame()
{
    auto renderSystem = GameManager::Instance().GetRenderSystem();
    auto spriteRenderSystem = GameManager::Instance().GetSpriteRenderSystem();
    if (!renderSystem || !spriteRenderSystem) return;

    if (m_gameCameraEntity != INVALID_ENTITY)
    {
        GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        renderSystem->RenderScene(m_gameCameraEntity, nullptr);
        spriteRenderSystem->Render();
    }
    else
    {
        GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

void GameScene::RenderEditor()
{
    auto renderSystem = GameManager::Instance().GetRenderSystem();
    auto spriteRenderSystem = GameManager::Instance().GetSpriteRenderSystem();
    if (!renderSystem || !spriteRenderSystem) return;

    // 2. Render to BackBuffer using Editor Camera
    GraphicsDevice::Instance().SetBackBuffer();
    GraphicsDevice::Instance().ClearBackBuffer(0.1f, 0.1f, 0.2f, 1.0f);
    if (m_editorCameraEntity != INVALID_ENTITY)
    {
        renderSystem->RenderScene(m_editorCameraEntity, nullptr);
        spriteRenderSystem->Render();
        CollisionManager::Instance().DrawDebugWires(0, 0, 1280.0f, 720.0f, m_editorCameraEntity);
    }

    Editor::Draw();
}



