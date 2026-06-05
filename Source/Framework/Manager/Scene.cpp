#include "Scene.h"
#include "../ECS/Components/TransformComponent.h"

Scene::Scene() {}
Scene::~Scene() {}

void Scene::Init() {
    // ECS?????????R???|?[?l???g?o?^??GameManager???S??
    // Scene??RenderSystem??Z?b?g?A?b?v??????s??
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
        if (obj->IsActive()) obj->Start();
    }
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) obj->Update();
    }
    for (auto& obj : m_gameObjects) {
        if (obj->IsActive()) obj->PostUpdate();
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

    // ECS Entity ???? GameManager ?o?R
    Entity id = GameManager::Instance().GetECS().CreateEntity();
    obj->SetEntityID(id);

    obj->AddComponent<TransformComponent>();

    m_gameObjects.push_back(obj);
    return obj;
}


