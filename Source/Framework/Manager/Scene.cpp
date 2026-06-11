#include "Scene.h"
#include "../ECS/Components/TransformComponent.h"
#include "CollisionManager.h"

Scene::Scene() {}
Scene::~Scene() {}

void Scene::Init() {
    // ECS初期化とコンポーネント登録はGameManagerが担当
    // SceneはRenderSystemのセットアップのみ行う
    auto& ecs = GameManager::Instance().GetECS();

    m_spRenderSystem = ecs.RegisterSystem<RenderSystem>();
    Signature renderSig;
    renderSig.set(ecs.GetComponentType<TransformData>());
    renderSig.set(ecs.GetComponentType<ModelRenderData>());
    ecs.SetSystemSignature<RenderSystem>(renderSig);
    m_spRenderSystem->m_pCoordinator = &ecs;
}

void Scene::Update() {
    CollisionManager::Instance().SetScene(this);
    CollisionManager::Instance().ClearDebugLines();
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) {
            obj->Update();
        }
    }

    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) {
            obj->PostUpdate();
        }
    }

    CollisionManager::Instance().Solve(this);

    // コリジョン解決後にすべてのTransformを再計算（子供にも反映させるため）
    std::function<void(const std::shared_ptr<GameObject>&)> updateTransforms = [&](const std::shared_ptr<GameObject>& obj) {
        if (!obj->IsActive()) return;
        if (auto pTrans = obj->GetComponent<TransformComponent>()) {
            pTrans->UpdateMatrix();
        }
        for (const auto& child : obj->GetChildren()) {
            updateTransforms(child);
        }
    };

    for (auto& obj : m_gameObjects) {
        updateTransforms(obj);
    }
}

void Scene::PreDraw() {
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) obj->PreDraw();
    }
}

void Scene::Draw() {
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) obj->Draw();
    }
}

void Scene::ImGuiUpdate() {
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) {
            obj->ImGuiUpdate();
        }
    }
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_shared<GameObject>();
    obj->SetName(name);
    obj->SetScene(this);

    // ECS EntityをGameManager経由で生成
    Entity id = GameManager::Instance().GetECS().CreateEntity();
    obj->SetEntityID(id);

    obj->AddComponent<TransformComponent>();

    m_gameObjects.push_back(obj);
    // 逆引きマップに登録
    m_entityToObject[id] = obj;
    return obj;
}

