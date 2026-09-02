#include "../../../Pch.h"
#include "GameScene.h"
#include "../../../Framework/Manager/GameManager.h"
#include "../../../Framework/ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/TransformSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/CameraSystem.h"
#include "../../../Framework/ECS/CompSystem/Systems/AnimationSystem.h"
#include "../../Object/Script/Player/Player.h"
#include "../../../Framework/Manager/Collision/CollisionManager.h"
#include "../../../Framework/Manager/NavMesh/NavMeshManager.h"
#include "../../Object/Script/System/GameSequence.h"
#include "../../../Framework/System/JobSystem/JobSystem.h"
#include "../../../Framework/ImGuiEditor/Editor/Editor.h"
#include "../../../Framework/ECS/CompSystem/Systems/ScriptSystem.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/Shader/ShaderManager/ShaderManager.h"
#include "../../../Graphics/Shader/ShaderLibrary.h"
#include "../../../Graphics/Shader/PostProcessShader/PostProcessShader.h"
#include "../../../Graphics/Shader/BloomShader/BloomShader.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"

void GameScene::Init()
{
    auto pDevice = GraphicsDevice::Instance().GetDevice();

    // GameManager::Init()??Application::Execute()??N??????1?????????z??B
    // (????????ECS?S????V???????????????A??????SEntityID????????????????????
    //  ????????????B???????????????GameManager::Init()???K?[?h??????????A
    //  ????????????????????????)?B
    Editor::Init();

    // Editor::Init()??Editor::s_scene???V????Scene?I?u?W?F?N?g?????????B
    // ????^?C?~???O??u?O??V?[???v???????????GameObject?Q??shared_ptr??Q??J?E???g??
    // 0??????j?????????AGameObject??f?X?g???N?^??ECS?R?}???h?o?b?t?@??
    // DestroyEntity?????????????????f???????B
    // ?????Deserialize()/Scene::Init()(Awake/Start)??V?????V?[?????????????_???A
    // ????j???R?}???h?????f??????????u?O??V?[??????????R???|?[?l???g?v??
    // ?V?V?[??????R?[?h??????????A????j????????????w???|?C???^?o?R??
    // ?N???b?V????????????(SceneManager????Flush?????A?????Editor::s_scene????
    // ?????????????????^?C?~???O?I?????????????)?B
    GameManager::Instance().GetECS().FlushCommands();

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
            auto& camTrans = cameraObj->GetComponent<TransformData>();
            camTrans.m_position = Math::Vector3(0, 0, -5.0f);
            cameraObj->AddComponent<CameraData>(CameraData{});
        }
    }

    Editor::GetScene()->Init(); // ????????[?h?????S?X?N???v?g??Awake/Start??????
    JobSystem::Instance().Wait();

    // m_fullscreenGame??true?????????J?n???AHandleModeSwitch()??F5?g???K??????
    // ?????????}?E?X???[?h??Absolute???????B??Player??FPS?????????J?n?????邽??A
    // ????????Relative???[?h???Z?b?g???Ă���
    if (m_fullscreenGame) {
        Input::Instance().SetMouseModeRelative();
    }
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

    TryBuildNavMesh();
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

                // ?G?f?B?^?J??????X?N???v?g??R???C?_?[?????????????????????C??(?t???[?J????????????????)
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

        // ?G?f?B?^?J????(MainCamera)??Q?[???J??????????g???????A????Q?[???v???C??????????X?i?b?v?????
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
                Math::Vector3 pivotOffset(0, 1.0f, 0); // ?v???C???[??g?????S?????

                // ?????????J???????????????????
                // (Player??NativeScript??ECS????????Component?^??????o?^?????????????A
                //  TryGetComponent<Player>()??g??????BNativeScriptData?o?R??dynamic_cast????)
                if (auto* pScriptData = ecs.TryGetComponent<NativeScriptData>(pPlayerObj->GetEntityID())) {
                    if (auto* pPlayerScript = dynamic_cast<Player*>(pScriptData->Instance.get())) {
                        const float kCrouchCameraLower = 0.5f; // ?????????J????????????
                        pivotOffset.y -= pPlayerScript->GetCrouchAmount() * kCrouchCameraLower;
                    }
                }

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

void GameScene::TryBuildNavMesh()
{
    if (NavMeshManager::Instance().IsBuilt()) return;

    auto gs = GameSequence::GetInstance();
    if (!gs) return; // GameSequence::Start() has not populated the room list yet, retry next frame

    for (RoomArea* room : gs->GetRooms()) {
        if (!room) continue;
        auto pObj = room->GetGameObject();
        Logger::Instance().AddLog(Logger::LogLevel::Info,
            "[GameScene] Room bounds: %s min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)",
            pObj ? pObj->GetName().c_str() : "?",
            room->m_min.x, room->m_min.y, room->m_min.z,
            room->m_max.x, room->m_max.y, room->m_max.z);
    }

    if (NavMeshManager::Instance().BuildManualNavMesh(gs->GetRooms())) {
        Logger::Instance().AddLog(Logger::LogLevel::Info, "[GameScene] NavMesh (manual, room-based) built.");

        // Log reachability between every room pair, so problems are visible immediately in the log
        const auto& rooms = gs->GetRooms();
        for (size_t i = 0; i < rooms.size(); ++i) {
            if (!rooms[i]) continue;
            std::string reachableNames;
            for (size_t j = 0; j < rooms.size(); ++j) {
                if (i == j || !rooms[j]) continue;
                if (NavMeshManager::Instance().IsReachable(rooms[i]->GetCenter(), rooms[j]->GetCenter())) {
                    if (!reachableNames.empty()) reachableNames += ", ";
                    auto pObj = rooms[j]->GetGameObject();
                    reachableNames += pObj ? pObj->GetName() : "?";
                }
            }
            auto pSelfObj = rooms[i]->GetGameObject();
            std::string selfName = pSelfObj ? pSelfObj->GetName() : "?";
            Logger::Instance().AddLog(Logger::LogLevel::Info,
                "[GameScene] NavMesh room reachability: %s (y=%.1f) -> [%s]",
                selfName.c_str(), rooms[i]->GetCenter().y, reachableNames.c_str());
        }
    }
}

void GameScene::Render()
{
    Renderer::BeginFrame();

    auto* pCmdList = GraphicsDevice::Instance().GetCmdList();

    auto renderSystem = GameManager::Instance().GetRenderSystem();
    if (renderSystem) {
        PROFILE_CPU_SCOPE("Shadow");
        PROFILE_GPU_SCOPE(pCmdList, "Shadow");
        renderSystem->RenderShadow();
    }

    if (m_fullscreenGame) {
        RenderGame();
        Editor::DrawProfilerOverlay(); // F3 toggles this - editor mode already shows it via RenderEditor()'s full UI
    }
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
        auto* pCmdList = GraphicsDevice::Instance().GetCmdList();

        auto* pSceneHDR = Renderer::GetSceneHDRRenderTarget();
        GraphicsDevice::Instance().SetRenderTarget(pSceneHDR);
        pSceneHDR->Clear(0.0f, 0.0f, 0.0f, 1.0f);

        {
            PROFILE_CPU_SCOPE("Reflection");
            PROFILE_GPU_SCOPE(pCmdList, "Reflection");
            renderSystem->RenderReflection(m_gameCameraEntity);
        }
        {
            PROFILE_CPU_SCOPE("Scene");
            PROFILE_GPU_SCOPE(pCmdList, "Scene");
            renderSystem->RenderScene(m_gameCameraEntity, pSceneHDR);
        }
        {
            PROFILE_CPU_SCOPE("Sprite");
            PROFILE_GPU_SCOPE(pCmdList, "Sprite");
            spriteRenderSystem->Render();
        }

        // Script Draw (Player UI, GameClear/Over ?\?????)
        // ?? DebugWire??Q?[?????[?h????\??
        auto scriptSystem = GameManager::Instance().GetScriptSystem();
        if (scriptSystem) {
            PROFILE_CPU_SCOPE("ScriptDraw");
            PROFILE_GPU_SCOPE(pCmdList, "ScriptDraw");
            scriptSystem->Draw();
        }

        // Phase 4: PostProcess (Bloom & ToneMapping)
        PROFILE_CPU_SCOPE("PostProcess");
        PROFILE_GPU_SCOPE(pCmdList, "PostProcess");

        auto& bloomShader = ShaderLibrary::Instance().Get<BloomShader>();
        auto& postProcessShader = ShaderLibrary::Instance().Get<PostProcessShader>();

        auto* pExtractRT = Renderer::GetBloomExtractRenderTarget();
        auto* pBlurRT0 = Renderer::GetBloomBlurRenderTarget(0);
        auto* pBlurRT1 = Renderer::GetBloomBlurRenderTarget(1);

        const auto& postProcessData = ShaderManager::Instance().GetPostProcessData();

        // 1. Bloom Extract
        GraphicsDevice::Instance().TransitionToSRV(pSceneHDR);
        bloomShader.DrawExtract(pSceneHDR, pExtractRT, postProcessData);

        // 2. Bloom Blur
        // Horizontal
        GraphicsDevice::Instance().TransitionToSRV(pExtractRT);
        bloomShader.DrawBlur(pExtractRT, pBlurRT0, postProcessData, 1.0f, 0.0f);
        // Vertical
        GraphicsDevice::Instance().TransitionToSRV(pBlurRT0);
        bloomShader.DrawBlur(pBlurRT0, pBlurRT1, postProcessData, 0.0f, 1.0f);

        // 3. Composite to Back Buffer
        GraphicsDevice::Instance().TransitionToSRV(pBlurRT1);
        GraphicsDevice::Instance().SetBackBuffer();
        Renderer::BindDefaultViewport();
        GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
        postProcessShader.Draw(pSceneHDR, pBlurRT1, postProcessData);
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

    auto* pSceneHDR = Renderer::GetSceneHDRRenderTarget();
    GraphicsDevice::Instance().SetRenderTarget(pSceneHDR);
    pSceneHDR->Clear(0.1f, 0.1f, 0.2f, 1.0f);

    auto* pCmdList = GraphicsDevice::Instance().GetCmdList();

    if (m_editorCameraEntity != INVALID_ENTITY)
    {
        {
            PROFILE_CPU_SCOPE("Scene");
            PROFILE_GPU_SCOPE(pCmdList, "Scene");
            renderSystem->RenderScene(m_editorCameraEntity, pSceneHDR);
        }
        {
            PROFILE_CPU_SCOPE("Sprite");
            PROFILE_GPU_SCOPE(pCmdList, "Sprite");
            spriteRenderSystem->Render();
        }

        // Script Draw (?f?o?b?O???C???[???)
        auto scriptSystem = GameManager::Instance().GetScriptSystem();
        if (scriptSystem) {
            PROFILE_CPU_SCOPE("ScriptDraw");
            PROFILE_GPU_SCOPE(pCmdList, "ScriptDraw");
            scriptSystem->PreDraw();
            scriptSystem->Draw();
        }
        {
            PROFILE_CPU_SCOPE("DebugDraw");
            PROFILE_GPU_SCOPE(pCmdList, "DebugDraw");
            NavMeshManager::Instance().DrawDebugMesh();
            CollisionManager::Instance().DrawDebugWires(0, 0, 1280.0f, 720.0f, m_editorCameraEntity);
        }
    }

    // Post Process to Back Buffer
    {
        PROFILE_CPU_SCOPE("PostProcess");
        PROFILE_GPU_SCOPE(pCmdList, "PostProcess");

        auto& bloomShader = ShaderLibrary::Instance().Get<BloomShader>();
        auto& postProcessShader = ShaderLibrary::Instance().Get<PostProcessShader>();

        auto* pExtractRT = Renderer::GetBloomExtractRenderTarget();
        auto* pBlurRT0 = Renderer::GetBloomBlurRenderTarget(0);
        auto* pBlurRT1 = Renderer::GetBloomBlurRenderTarget(1);

        const auto& postProcessData = ShaderManager::Instance().GetPostProcessData();

        // 1. Bloom Extract
        GraphicsDevice::Instance().TransitionToSRV(pSceneHDR);
        bloomShader.DrawExtract(pSceneHDR, pExtractRT, postProcessData);

        // 2. Bloom Blur (Horizontal then Vertical)
        GraphicsDevice::Instance().TransitionToSRV(pExtractRT);
        bloomShader.DrawBlur(pExtractRT, pBlurRT0, postProcessData, 1.0f, 0.0f);
        GraphicsDevice::Instance().TransitionToSRV(pBlurRT0);
        bloomShader.DrawBlur(pBlurRT0, pBlurRT1, postProcessData, 0.0f, 1.0f);

        // 3. Composite
        GraphicsDevice::Instance().TransitionToSRV(pBlurRT1);
        GraphicsDevice::Instance().SetBackBuffer();
        Renderer::BindDefaultViewport();
        GraphicsDevice::Instance().ClearBackBuffer(0.1f, 0.1f, 0.2f, 1.0f);
        postProcessShader.Draw(pSceneHDR, pBlurRT1, postProcessData);
    }

    Editor::Draw();
}
