#include "../../Pch.h"
#include "GameManager.h"
#include "../../Graphics/Shader/ShaderLibrary.h"
#include "../../Graphics/Shader/StandardShader/StandardShader.h"
#include "../../Graphics/Shader/LitShader/LitShader.h"
#include "../../Graphics/Shader/ShadowShader/ShadowShader.h"
#include "../../Graphics/Shader/SkinningShader/SkinningShader.h"
#include "../../Graphics/Shader/PostProcessShader/PostProcessShader.h"
#include "Scene/Scene.h"
#include "../Object/GameObject.h"
#include "../ECS/CompSystem/Systems/RenderSystem.h"
#include "../ECS/CompSystem/SpriteRenderSystem/SpriteRenderSystem.h"
#include "../ECS/CompSystem/Systems/TransformSystem.h"
#include "../ECS/CompSystem/Systems/CameraSystem.h"
#include "../ECS/CompSystem/Systems/AnimationSystem.h"
#include "../ECS/CompSystem/Systems/ScriptSystem.h"

// GameManager 縺ｮ static 繝｡繝ｳ繝仙､画焚縺ｮ螳夂ｾｩ
bool GameManager::s_alive = true;
GameManager& GameManager::Instance()
{
    static GameManager instance;
    return instance;
}

void GameManager::Init()
{
    m_ecs.Init();

    // --- Component 蝙狗匳骭ｲ ---
    m_ecs.RegisterComponent<TransformData>();
    m_ecs.RegisterComponent<CameraData>();
    m_ecs.RegisterComponent<ModelRenderData>();
    m_ecs.RegisterComponent<ShaderData>();
    m_ecs.RegisterComponent<AnimationDataComponent>();
    m_ecs.RegisterComponent<ColliderData>();
    m_ecs.RegisterComponent<NativeScriptData>();
    m_ecs.RegisterComponent<SpriteData>();

    // --- System 逋ｻ骭ｲ & Signature 險ｭ螳・---
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
    ShaderLibrary::Instance().Register<StandardShader>(pDevice);
    ShaderLibrary::Instance().Register<LitShader>(pDevice);
    ShaderLibrary::Instance().Register<ShadowShader>(pDevice);
    ShaderLibrary::Instance().Register<SkinningShader>(pDevice);
    ShaderLibrary::Instance().Register<PostProcessShader>(pDevice);
}

void GameManager::Update(float deltaTime, class Scene* pScene)
{
    if (!pScene) return;

    m_spScriptSystem->Update(deltaTime);

    m_spAnimationSystem->Update(deltaTime);

    // Transform: 繝ｫ繝ｼ繝医が繝悶ず繧ｧ繧ｯ繝医°繧蛾嚴螻､譖ｴ譁ｰ
    std::vector<std::shared_ptr<class GameObject>> roots;
    for (auto& obj : pScene->GetGameObjects()) {
        if (!obj->GetParent()) roots.push_back(obj);
    }
    m_spTransformSystem->Update(roots);

    // Collision
    CollisionManager::Instance().SetScene(pScene);
    CollisionManager::Instance().ClearDebugLines();
    CollisionManager::Instance().Solve(pScene);

    // 陦晉ｪ∝ｾ後・ Transform 蜀肴峩譁ｰ
    m_spTransformSystem->Update(roots);

    m_spCameraSystem->Update(deltaTime);
    m_spScriptSystem->PostUpdate();

    GetECS().FlushCommands();
}


