#pragma once
#include "../Library/nlohmann/json.hpp"
#include "../Object/Object.h"
#include "../Object/GameObject.h"
#include "../ECS/CompSystem/Systems/RenderSystem.h"

// =============================================
// Scene
// GameObject????X?g??RenderSystem????????
// ECS??f?[?^?????GameManager???????
// =============================================
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

    void Serialize(nlohmann::json& out) const {
        nlohmann::json objs = nlohmann::json::array();
        for (auto& obj : m_gameObjects) {
            nlohmann::json oj;
            obj->Serialize(oj);
            objs.push_back(oj);
        }
        out["GameObjects"] = objs;
    }

    void Deserialize(const nlohmann::json& in) {
        m_gameObjects.clear();
        if (in.contains("GameObjects")) {
            for (const auto& oj : in["GameObjects"]) {
                auto obj = std::make_shared<GameObject>();
                obj->SetScene(this);
                Entity id = GameManager::Instance().GetECS().CreateEntity();
                obj->SetEntityID(id);
                m_gameObjects.push_back(obj);
                obj->Deserialize(oj);
            }
        }
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

    // RenderSystem ?? Scene ?????
    std::shared_ptr<class RenderSystem>& GetRenderSystem() { return m_spRenderSystem; }

protected:
    std::vector<std::shared_ptr<GameObject>> m_gameObjects;
    std::shared_ptr<class RenderSystem> m_spRenderSystem;
};

