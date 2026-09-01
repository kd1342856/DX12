#include "../../Pch.h"
#include "GameManager.h"
#include "../../Graphics/Shader/ShaderLibrary.h"
#include "../../Graphics/Shader/LitShader/LitShader.h"
#include "../../Graphics/Shader/ShadowShader/ShadowShader.h"
#include "../../Graphics/Shader/SkinningShader/SkinningShader.h"
#include "../../Graphics/Shader/PostProcessShader/PostProcessShader.h"
#include "../../Graphics/Shader/BloomShader/BloomShader.h"
#include "../../Graphics/Shader/SkyShader/SkyShader.h"
#include "../../Graphics/Shader/FogShader/FogShader.h"
#include "Scene/Scene.h"
#include "../Object/GameObject.h"
#include "../ECS/CompSystem/Systems/RenderSystem.h"
#include "../ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../ECS/CompSystem/Systems/TransformSystem.h"
#include "../ECS/CompSystem/Systems/CameraSystem.h"
#include "../ECS/CompSystem/Systems/AnimationSystem.h"
#include "../ECS/CompSystem/Systems/ScriptSystem.h"
#include "../ECS/ComponentSerializerRegistration.h"

// GameManager の static メンバ変数の定義
bool GameManager::s_alive = true;
GameManager& GameManager::Instance()
{
    static GameManager instance;
    return instance;
}

void GameManager::Init()
{
    // アプリ起動時に1回だけ呼ぶ想定。GameScene::Init()等から誤って毎回呼ばれると、
    // ECSCoordinator::Init()がEntity/Component/SystemManagerを丸ごと新しいものに
    // 差し替えてしまい、その時点で生きている全Entity参照(古いシーンのGameObjectや
    // 各種static参照等)が無効になって「存在しないEntity」で落ちる原因になっていた
    // (シーン切り替え時にクラッシュしていた不具合の根本原因)。
    // 二重初期化を防ぐガード。
    if (m_isInitialized) return;
    m_isInitialized = true;

    m_ecs.Init();
    RegisterComponentSerializers();

    // --- Component 型登録 ---
    m_ecs.RegisterComponent<TransformData>();
    m_ecs.RegisterComponent<CameraData>();
    m_ecs.RegisterComponent<ModelRenderData>();
    m_ecs.RegisterComponent<ShaderData>();
    m_ecs.RegisterComponent<AnimationDataComponent>();
    m_ecs.RegisterComponent<ColliderData>();
    m_ecs.RegisterComponent<NativeScriptData>();
    m_ecs.RegisterComponent<SpriteData>();

    // --- System 登録 & Signature 設定 ---
    m_spScriptSystem = m_ecs.RegisterSystem<ScriptSystem>();
    {
        Signature sig;
        sig.set(m_ecs.GetComponentType<NativeScriptData>());
        m_ecs.SetSystemSignature<ScriptSystem>(sig);
        m_spScriptSystem->m_pCoordinator = &m_ecs;
    }

    m_spTransformSystem = m_ecs.RegisterSystem<TransformSystem>();
    {
        Signature sig;
        sig.set(m_ecs.GetComponentType<TransformData>());
        m_ecs.SetSystemSignature<TransformSystem>(sig);
        m_spTransformSystem->m_pCoordinator = &m_ecs;
    }

    m_spCameraSystem = m_ecs.RegisterSystem<CameraSystem>();
    {
        Signature sig;
        sig.set(m_ecs.GetComponentType<TransformData>());
        sig.set(m_ecs.GetComponentType<CameraData>());
        m_ecs.SetSystemSignature<CameraSystem>(sig);
        m_spCameraSystem->m_pCoordinator = &m_ecs;
    }

    m_spAnimationSystem = m_ecs.RegisterSystem<AnimationSystem>();
    {
        Signature sig;
        sig.set(m_ecs.GetComponentType<AnimationDataComponent>());
        sig.set(m_ecs.GetComponentType<ModelRenderData>());
        m_ecs.SetSystemSignature<AnimationSystem>(sig);
        m_spAnimationSystem->m_pCoordinator = &m_ecs;
    }

    m_spRenderSystem = m_ecs.RegisterSystem<RenderSystem>();
    {
        Signature sig;
        sig.set(m_ecs.GetComponentType<TransformData>());
        sig.set(m_ecs.GetComponentType<ModelRenderData>());
        m_ecs.SetSystemSignature<RenderSystem>(sig);
        m_spRenderSystem->m_pCoordinator = &m_ecs;
    }

    m_spSpriteRenderSystem = m_ecs.RegisterSystem<SpriteRenderSystem>();
    {
        Signature sig;
        sig.set(m_ecs.GetComponentType<TransformData>());
        sig.set(m_ecs.GetComponentType<SpriteData>());
        m_ecs.SetSystemSignature<SpriteRenderSystem>(sig);
        m_spSpriteRenderSystem->m_pCoordinator = &m_ecs;
    }

    CollisionManager::Instance().Init();
        JobSystem::Instance().Init();

    auto* pDevice = &GDF::Instance().GetGraphicsDevice();
    ShaderLibrary::Instance().Register<LitShader>(pDevice);
    ShaderLibrary::Instance().Register<ShadowShader>(pDevice);
    ShaderLibrary::Instance().Register<SkinningShader>(pDevice);
    ShaderLibrary::Instance().Register<PostProcessShader>(pDevice);
    ShaderLibrary::Instance().Register<BloomShader>(pDevice);
    ShaderLibrary::Instance().Register<SkyShader>(pDevice);
    ShaderLibrary::Instance().Register<FogShader>(pDevice);
}

void GameManager::Update(float deltaTime, class Scene* pScene)
{
    GetECS().FlushCommands();

    if (!pScene) return;

    m_spScriptSystem->Update(deltaTime);

    m_spAnimationSystem->Update(deltaTime);

    // Transform: ルートオブジェクトから階層更新
    std::vector<std::shared_ptr<class GameObject>> roots;
    for (auto& obj : pScene->GetGameObjects()) {
        if (!obj->GetParent()) roots.push_back(obj);
    }
    m_spTransformSystem->Update(roots);

    // Collision
    CollisionManager::Instance().SetScene(pScene);
    CollisionManager::Instance().ClearDebugLines();
    CollisionManager::Instance().Solve(pScene);

    // 衝突後の Transform 再更新
    m_spTransformSystem->Update(roots);

    m_spCameraSystem->Update(deltaTime);
    m_spScriptSystem->PostUpdate();

    GetECS().FlushCommands();
}

void GameManager::Shutdown()
{
    if (!s_alive) return;
    s_alive = false;
    
    // 全エンティティとコンポーネントを安全に破棄
    m_ecs.Shutdown();
    
    JobSystem::Instance().Shutdown();
}


