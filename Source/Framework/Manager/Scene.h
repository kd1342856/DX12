#pragma once
#include "../../../Library/nlohmann/json.hpp"
#include "../Object/Object.h"
#include "../Object/GameObject.h"
class RenderSystem;
class SpriteRenderSystem;
class TransformSystem;
class CameraSystem;
class AnimationSystem;
class ScriptSystem;

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
    nlohmann::json SerializeGameObject(std::shared_ptr<GameObject> obj) const;
    void Deserialize(const nlohmann::json& in);
    void DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<class GameObject> parent);

    void RegisterGameObject(Entity e, std::shared_ptr<GameObject> obj) {
        m_entityToObject[e] = obj;
    }

    std::shared_ptr<GameObject> CreateGameObject(const std::string& name = "GameObject");

    // Prefab Instantiate
    std::shared_ptr<GameObject> Instantiate(const std::string& filepath, const Math::Vector3& position = Math::Vector3::Zero);

    // Destroy
    void Destroy(std::shared_ptr<GameObject> obj);

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
    std::shared_ptr<class SpriteRenderSystem>& GetSpriteRenderSystem() { return m_spSpriteRenderSystem; }

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
    std::shared_ptr<class SpriteRenderSystem> m_spSpriteRenderSystem;
    std::shared_ptr<class TransformSystem> m_spTransformSystem;
    std::shared_ptr<class CameraSystem> m_spCameraSystem;
    std::shared_ptr<class AnimationSystem> m_spAnimationSystem;
    std::shared_ptr<class ScriptSystem> m_spScriptSystem;
};
