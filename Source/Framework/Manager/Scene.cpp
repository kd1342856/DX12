#include "Scene.h"

Scene::Scene() {}
Scene::~Scene() {}

void Scene::Init() {
    // ECSの初期化とコンポーネント登録はGameManagerが担当
    // SceneはRenderSystemのセットアップだけを行う
    auto& ecs = GameManager::Instance().GetECS();

    m_spRenderSystem = ecs.RegisterSystem<RenderSystem>();
    Signature renderSig;
    renderSig.set(ecs.GetComponentType<TransformData>());
    renderSig.set(ecs.GetComponentType<ModelRenderData>());
    ecs.SetSystemSignature<RenderSystem>(renderSig);
    m_spRenderSystem->m_pCoordinator = &ecs;
}

void Scene::Update() {
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) {
            obj->Update();
        }
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

    // ECS Entity 作成は GameManager 経由
    Entity id = GameManager::Instance().GetECS().CreateEntity();
    obj->SetEntityID(id);

    m_gameObjects.push_back(obj);
    return obj;
}