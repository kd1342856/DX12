#pragma once
#include "../../../Library/nlohmann/json.hpp"
#include "../Object/Object.h"
#include "../Object/GameObject.h"
#include "../ECS/CompSystem/Systems/RenderSystem.h"
#include "../ECS/CompSystem/Systems/TransformSystem.h"
#include "../ECS/CompSystem/Systems/CameraSystem.h"
#include "../ECS/CompSystem/Systems/AnimationSystem.h"
#include "../ECS/CompSystem/Systems/ScriptSystem.h"

class Scene : public Object {
public:
    Scene();
    ~Scene();

    void Init();
    void Update();
    void PreDraw();
    void Draw();
    void ImGuiUpdate();

    const std::vector<std::shared_ptr<GameObject>>& GetGameObjects() const { return m_gameObjects; }

    void Serialize(nlohmann::json& out) const;
    void Deserialize(const nlohmann::json& in);
    void DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<class GameObject> parent);

    void RegisterGameObject(Entity e, std::shared_ptr<GameObject> obj) {
        m_entityToObject[e] = obj;
    }

    std::shared_ptr<GameObject> CreateGameObject(const std::string& name = "GameObject");

    void AddGameObject(std::shared_ptr<GameObject> obj) {
        auto it = std::find(m_gameObjects.begin(), m_gameObjects.end(), obj);
        if (it == m_gameObjects.end()) {
            m_gameObjects.push_back(obj);
        }
    }

    void RemoveGameObject(std::shared_ptr<GameObject> obj) {
        auto it = std::find(m_gameObjects.begin(), m_gameObjects.end(), obj);
        if (it != m_gameObjects.end()) {
            m_gameObjects.erase(it);
        }
    }

    std::shared_ptr<class RenderSystem>& GetRenderSystem() { return m_spRenderSystem; }

protected:
    std::vector<std::shared_ptr<GameObject>> m_gameObjects;

public:
    std::shared_ptr<GameObject> GetGameObject(Entity e) {
        auto it = m_entityToObject.find(e);
        if (it != m_entityToObject.end()) return it->second;
        return nullptr;
    }

private:
    std::unordered_map<Entity, std::shared_ptr<GameObject>> m_entityToObject;
    std::shared_ptr<class RenderSystem> m_spRenderSystem;
    std::shared_ptr<class TransformSystem> m_spTransformSystem;
    std::shared_ptr<class CameraSystem> m_spCameraSystem;
    std::shared_ptr<class AnimationSystem> m_spAnimationSystem;
    std::shared_ptr<class ScriptSystem> m_spScriptSystem;
};