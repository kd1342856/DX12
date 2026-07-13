#include "../../Pch.h"
#include "GameManager.h"
#include "Scene/Scene.h"
#include "../Object/GameObject.h"
#include "../ECS/CompSystem/Systems/RenderSystem.h"
#include "../ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../ECS/CompSystem/Systems/TransformSystem.h"
#include "../ECS/CompSystem/Systems/CameraSystem.h"
#include "../ECS/CompSystem/Systems/AnimationSystem.h"
#include "../ECS/CompSystem/Systems/ScriptSystem.h"

// GameManager の static メンバ変数の定義
bool GameManager::s_alive = true;
GameManager& GameManager::Instance()
{
    static GameManager instance;
    return instance;
}

void GameManager::Init()
{
    m_ecs.Init();

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
}

void GameManager::Update(float deltaTime, class Scene* pScene)
{
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

    pScene->FlushDestroy();
}
