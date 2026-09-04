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
#include "../../../Graphics/Shader/GodRaysShader/GodRaysShader.h"
#include "../../../Graphics/Shader/SSAOShader/SSAOShader.h"
#include "../../../Framework/DirectX/Utility/Profiler.h"
#include "../../../Framework/ECS/CompSystem/Systems/LightSystem.h"
#include <algorithm>

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
        Editor::DrawProfilerOverlay(); // F3で切り替え - エディタモードではRenderEditor()のフルUIで既に表示される
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

        UpdateLights(m_gameCameraEntity);
        UpdatePointLightShadow(m_gameCameraEntity);
        RenderSSAO(m_gameCameraEntity);

        // ReflectionComponent::PreDraw()がここで平面の位置/法線をワールド空間へ変換する。
        // 以前はRenderGame()経路(実際のゲームプレイ=FPSカメラ)でこれが一度も呼ばれておらず、
        // RenderReflection()が初期値(point=(0,0,0), normal=(0,0,1))のまま鏡を描画していた
        // (ゲームプレイ中の反射が実際の窓の位置・向きを無視していた原因)。
        auto scriptSystemPreDraw = GameManager::Instance().GetScriptSystem();
        if (scriptSystemPreDraw) {
            PROFILE_CPU_SCOPE("ScriptPreDraw");
            PROFILE_GPU_SCOPE(pCmdList, "ScriptPreDraw");
            scriptSystemPreDraw->PreDraw();
        }

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

        // Phase 4: PostProcess (Bloom, DOF, ToneMapping, Vignette, Grain, Chromatic Aberration...)
        DoPostProcess(m_gameCameraEntity, 0.0f, 0.0f, 0.0f);
    }
    else
    {
        GraphicsDevice::Instance().ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

void GameScene::UpdateLights(Entity cameraEntity)
{
    auto lightSystem = GameManager::Instance().GetLightSystem();
    if (!lightSystem) return;

    Math::Vector3 camPos = Math::Vector3::Zero;
    auto& ecs = GameManager::Instance().GetECS();
    if (auto* pTrans = ecs.TryGetComponent<TransformData>(cameraEntity))
    {
        camPos = pTrans->m_worldMatrix.Translation();
    }
    lightSystem->Update(GameTimer::Instance().DeltaTime(), camPos);
}

void GameScene::UpdatePointLightShadow(Entity cameraEntity)
{
    auto renderSystem = GameManager::Instance().GetRenderSystem();
    if (!renderSystem) return;

    const auto& lightData = ShaderManager::Instance().GetLightData();
    if (lightData.PL_Count <= 0)
    {
        ShaderManager::Instance().SetPointLightShadowData(Math::Matrix::Identity, 0.0f, false);
        return;
    }

    // g_PL[0]はLightSystemが視点に近い順でソート済みなので、そのまま最も近いライトを使う。
    Math::Vector3 lightPos = lightData.PL[0].Pos;
    float range = lightData.PL[0].Range;

    Math::Vector3 aimAt = lightPos + Math::Vector3(0, -1, 0);
    auto& ecs = GameManager::Instance().GetECS();
    if (cameraEntity != INVALID_ENTITY)
    {
        if (auto* pTrans = ecs.TryGetComponent<TransformData>(cameraEntity))
        {
            aimAt = pTrans->m_worldMatrix.Translation();
        }
    }

    renderSystem->RenderPointLightShadow(lightPos, aimAt, range);
}

void GameScene::RenderSSAO(Entity cameraEntity)
{
    auto renderSystem = GameManager::Instance().GetRenderSystem();
    if (!renderSystem || cameraEntity == INVALID_ENTITY) return;

    auto& ssaoSettings = ShaderManager::Instance().GetSSAOSettings();
    auto* pSSAORaw = Renderer::GetSSAORenderTarget(0);
    auto* pSSAOBlur = Renderer::GetSSAORenderTarget(1);

    if (!ssaoSettings.EnableSSAO)
    {
        // 無効時はAO=1(遮蔽なし)を維持する。LitShaderは毎フレームこのRT(index0)を読むため。
        GraphicsDevice::Instance().SetRenderTarget(pSSAORaw);
        pSSAORaw->Clear(1.0f, 1.0f, 1.0f, 1.0f);
        GraphicsDevice::Instance().TransitionToSRV(pSSAORaw);
        return;
    }

    auto* pNormalRT = Renderer::GetNormalPrepassRenderTarget();
    renderSystem->RenderNormalPrepass(cameraEntity, pNormalRT);

    auto& ssaoShader = ShaderLibrary::Instance().Get<SSAOShader>();
    SSAOShader::Params params;
    params.Radius = ssaoSettings.Radius;
    params.Bias = ssaoSettings.Bias;
    params.Power = ssaoSettings.Power;
    params.Intensity = ssaoSettings.Intensity;
    ssaoShader.Draw(pNormalRT, pSSAORaw, params);

    // Bloomのブラーパイプラインを再利用して軽くぼかす(SSAO特有のノイズを均す)。
    // index0(raw)→index1(H)→index0(V)の順で処理するので、最終結果は必ずindex0に戻る
    // (LitShader.cppのt12バインドがindex0を読む前提と対応させている)。
    auto& bloomShader = ShaderLibrary::Instance().Get<BloomShader>();
    CBufferData::PostProcess blurData = {};
    blurData.BlurRadius = 1.5f;
    GraphicsDevice::Instance().TransitionToSRV(pSSAORaw);
    bloomShader.DrawBlur(pSSAORaw, pSSAOBlur, blurData, 1.0f, 0.0f);
    GraphicsDevice::Instance().TransitionToSRV(pSSAOBlur);
    bloomShader.DrawBlur(pSSAOBlur, pSSAORaw, blurData, 0.0f, 1.0f);
    GraphicsDevice::Instance().TransitionToSRV(pSSAORaw);
}

void GameScene::DoPostProcess(Entity cameraEntity, float clearR, float clearG, float clearB)
{
    auto* pCmdList = GraphicsDevice::Instance().GetCmdList();
    PROFILE_CPU_SCOPE("PostProcess");
    PROFILE_GPU_SCOPE(pCmdList, "PostProcess");

    auto* pSceneHDR = Renderer::GetSceneHDRRenderTarget();
    auto& bloomShader = ShaderLibrary::Instance().Get<BloomShader>();
    auto& postProcessShader = ShaderLibrary::Instance().Get<PostProcessShader>();

    // DOFの深度リニア化に使うNear/Farを、実際に描画したカメラの値へ同期
    if (cameraEntity != INVALID_ENTITY)
    {
        auto& ecs = GameManager::Instance().GetECS();
        if (auto* pCamData = ecs.TryGetComponent<CameraData>(cameraEntity))
        {
            ShaderManager::Instance().SetCameraNearFar(pCamData->m_nearZ, pCamData->m_farZ);
        }
    }

    const auto& postProcessSettings = ShaderManager::Instance().GetPostProcessSettings();
    const auto& postProcessData = ShaderManager::Instance().GetPostProcessData();

    // 1. Bloom Extract (輝度が閾値を超えた部分だけ抽出)
    auto* pExtractRT = Renderer::GetBloomExtractRenderTarget();
    GraphicsDevice::Instance().TransitionToSRV(pSceneHDR);
    bloomShader.DrawExtract(pSceneHDR, pExtractRT, postProcessData);

    // 2. Bloom Blur - 半径(BloomRadius)と反復回数(BloomIterations)はShaderEditorから調整可能。
    // 以前は1/2解像度+固定半径5texel+1回だけだったため、Intensityを上げてもほぼ変化が
    // 見えなかった。1/4解像度化+可変半径+複数回ブラーで画面に広がる量を確保する。
    auto* pBlurRT0 = Renderer::GetBloomBlurRenderTarget(0);
    auto* pBlurRT1 = Renderer::GetBloomBlurRenderTarget(1);
    RenderTarget* pBloomResult = pExtractRT;
    int bloomIterations = std::max(1, postProcessSettings.BloomIterations);
    for (int i = 0; i < bloomIterations; ++i)
    {
        GraphicsDevice::Instance().TransitionToSRV(pBloomResult);
        bloomShader.DrawBlur(pBloomResult, pBlurRT0, postProcessData, 1.0f, 0.0f); // Horizontal
        GraphicsDevice::Instance().TransitionToSRV(pBlurRT0);
        bloomShader.DrawBlur(pBlurRT0, pBlurRT1, postProcessData, 0.0f, 1.0f);     // Vertical
        pBloomResult = pBlurRT1;
    }

    // 3. Depth of Field - 有効な時だけ、シーン全体(閾値なし)をぼかしたコピーを作る。
    // BloomのExtract/Blurパスをそのまま閾値0で使い回しているので専用シェーダーは不要。
    auto* pDofRT0 = Renderer::GetDOFBlurRenderTarget(0);
    RenderTarget* pDofResult = pDofRT0;
    if (postProcessSettings.EnableDOF)
    {
        auto* pDofRT1 = Renderer::GetDOFBlurRenderTarget(1);

        CBufferData::PostProcess dofExtractData = postProcessData;
        dofExtractData.BloomThreshold = 0.0f; // 閾値0 = 素通し(シーン全体をそのままコピー)

        GraphicsDevice::Instance().TransitionToSRV(pSceneHDR);
        bloomShader.DrawExtract(pSceneHDR, pDofRT0, dofExtractData);

        GraphicsDevice::Instance().TransitionToSRV(pDofRT0);
        bloomShader.DrawBlur(pDofRT0, pDofRT1, dofExtractData, 1.0f, 0.0f);
        GraphicsDevice::Instance().TransitionToSRV(pDofRT1);
        bloomShader.DrawBlur(pDofRT1, pDofRT0, dofExtractData, 0.0f, 1.0f);
    }
    GraphicsDevice::Instance().TransitionToSRV(pDofResult);

    // 4. God Rays - 平行光をスクリーン空間に投影した位置から、Bloom結果をラジアルブラーして
    // 光条を作る。新しいシャドウマップ等は使わない軽量な近似。
    auto* pGodRaysRT = Renderer::GetGodRaysRenderTarget();
    if (postProcessSettings.EnableGodRays)
    {
        auto& context = Renderer::GetContext();
        Math::Matrix vp = context.View * context.Projection;
        Math::Vector3 camPos = context.View.Invert().Translation();
        Math::Vector3 dlDir = ShaderManager::Instance().GetLightData().DL_Dir; // シーンへ向かう方向
        Math::Vector3 towardLight = -dlDir;
        Math::Vector3 farPoint = camPos + towardLight * 5000.0f;

        Math::Vector4 clip = Math::Vector4::Transform(Math::Vector4(farPoint.x, farPoint.y, farPoint.z, 1.0f), vp);
        bool lightValid = clip.w > 0.001f;
        float u = 0.5f, v = 0.5f;
        if (lightValid)
        {
            u = (clip.x / clip.w) * 0.5f + 0.5f;
            v = 1.0f - ((clip.y / clip.w) * 0.5f + 0.5f);
        }
        ShaderManager::Instance().SetGodRaysLightScreenPos(u, v, lightValid);

        if (lightValid)
        {
            auto& godRaysShader = ShaderLibrary::Instance().Get<GodRaysShader>();
            // g_EnableGodRaysはSetGodRaysLightScreenPos直後のUpdateConstantBuffersで確定するため、
            // ここではpostProcessDataではなく最新のGetPostProcessData()を使う。
            const auto& godRaysData = ShaderManager::Instance().GetPostProcessData();
            GraphicsDevice::Instance().TransitionToSRV(pBloomResult);
            godRaysShader.Draw(pBloomResult, pGodRaysRT, godRaysData);
            GraphicsDevice::Instance().TransitionToSRV(pGodRaysRT);
        }
    }
    else
    {
        ShaderManager::Instance().SetGodRaysLightScreenPos(0.5f, 0.5f, false);
    }

    // 5. Composite to Back Buffer
    // pSceneHDR自身の深度バッファをDOFのCoC計算用にSRVへ遷移(RenderScene終了時に書き込み用へ
    // 戻されているため)。Draw後はDEPTH_WRITEへ戻し、次フレームの描画に備える。
    GraphicsDevice::Instance().TransitionDepthToSRV(pSceneHDR);
    GraphicsDevice::Instance().TransitionToSRV(pBloomResult);
    GraphicsDevice::Instance().SetBackBuffer();
    Renderer::BindDefaultViewport();
    GraphicsDevice::Instance().ClearBackBuffer(clearR, clearG, clearB, 1.0f);
    postProcessShader.Draw(pSceneHDR, pBloomResult, postProcessData, pDofResult, pGodRaysRT);
    GraphicsDevice::Instance().TransitionDepthToWrite(pSceneHDR);
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
        UpdateLights(m_editorCameraEntity);
        UpdatePointLightShadow(m_editorCameraEntity);
        RenderSSAO(m_editorCameraEntity);

        // ReflectionComponent::PreDraw()がここで平面の位置/法線をワールド空間へ変換する。
        // RenderReflection()より後に呼んでいると1フレーム遅れた(古い)平面情報を使ってしまうので、
        // 先に呼んでおく(以前はScriptDraw部分でRenderScene/Sprite描画より後に呼ばれていた)。
        auto scriptSystem = GameManager::Instance().GetScriptSystem();
        if (scriptSystem) {
            PROFILE_CPU_SCOPE("ScriptPreDraw");
            PROFILE_GPU_SCOPE(pCmdList, "ScriptPreDraw");
            scriptSystem->PreDraw();
        }

        {
            // 以前はRenderGame()経由(FPSカメラ)でしか呼ばれておらず、エディタのフリーカメラでは
            // 反射テクスチャが直前のゲームプレイ時点のまま凍結していた(反射のデバッグが
            // 事実上できなかった原因)。フリーカメラでも都度更新するようにする。
            PROFILE_CPU_SCOPE("Reflection");
            PROFILE_GPU_SCOPE(pCmdList, "Reflection");
            renderSystem->RenderReflection(m_editorCameraEntity);
        }
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

        // Script Draw (デバッグワイヤー等表示)
        if (scriptSystem) {
            PROFILE_CPU_SCOPE("ScriptDraw");
            PROFILE_GPU_SCOPE(pCmdList, "ScriptDraw");
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
    DoPostProcess(m_editorCameraEntity, 0.1f, 0.1f, 0.2f);

    Editor::Draw();
}
