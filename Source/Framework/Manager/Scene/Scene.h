#pragma once
#include "../../../../Library/nlohmann/json.hpp"
#include "../../Object/Object.h"
#include "../../Object/GameObject.h"
#include "./SceneBase.h"

// Scene 縺ｯ System 繧剃ｸ蛻・衍繧峨↑縺・・
// System 縺ｮ逋ｻ骭ｲ繝ｻUpdate 縺ｯ GameManager 縺御ｸ蜈・ｮ｡逅・☆繧九・
class Scene : public SceneBase {
public:
    Scene();
    ~Scene();

    void Init()   override;
    void Update(float deltaTime) override;
    void Load()   override {}
    void Unload() override {}
    void Draw()   override;
    void ImGuiUpdate();

    const std::vector<std::shared_ptr<GameObject>>& GetGameObjects() const { return m_gameObjects; }

    // 繧ｷ繝ｪ繧｢繝ｩ繧､繧ｺ
    void Serialize(nlohmann::json& out) const;
    nlohmann::json SerializeGameObject(std::shared_ptr<GameObject> obj) const;
    void Deserialize(const nlohmann::json& in);
    void DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<class GameObject> parent);

    // Entity 竊・GameObject 縺ｮ繝槭ャ繝斐Φ繧ｰ逋ｻ骭ｲ
    void RegisterGameObject(Entity e, std::shared_ptr<GameObject> obj) {
        m_entityToObject[e] = obj;
    }

    // 逕滓・繝ｻ遐ｴ譽・
    std::shared_ptr<GameObject> CreateGameObject(const std::string& name = "GameObject");
    std::shared_ptr<GameObject> Instantiate(const std::string& filepath, const Math::Vector3& position = Math::Vector3::Zero);
    
    

    // 蜀・Κ繝ｪ繧ｹ繝域桃菴・
    void AddGameObject(std::shared_ptr<GameObject> obj) {
        auto it = std::find(m_gameObjects.begin(), m_gameObjects.end(), obj);
        if (it == m_gameObjects.end()) {
            m_gameObjects.push_back(obj);
        }
    }

    void RemoveGameObject(std::shared_ptr<GameObject> obj) {
        Entity e = obj->GetEntityID();
        auto it = std::find(m_gameObjects.begin(), m_gameObjects.end(), obj);
        if (it != m_gameObjects.end()) {
            m_gameObjects.erase(it);
        }
        // 繝ｪ繝ｼ繧ｯ菫ｮ豁｣: Entity -> shared_ptr 縺ｮ繝槭ャ繝斐Φ繧ｰ繧ょｿ・★豸医☆
        m_entityToObject.erase(e);
    }

    std::shared_ptr<GameObject> GetGameObject(Entity e) {
        auto it = m_entityToObject.find(e);
        if (it != m_entityToObject.end()) return it->second;
        return nullptr;
    }

protected:
    std::vector<std::shared_ptr<GameObject>> m_gameObjects;

private:
    std::unordered_map<Entity, std::shared_ptr<GameObject>> m_entityToObject;
    
};

