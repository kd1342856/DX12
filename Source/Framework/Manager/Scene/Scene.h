#pragma once
#include "../../../../Library/nlohmann/json.hpp"
#include "../../Object/Object.h"
#include "../../Object/GameObject.h"
#include "./SceneBase.h"

// Scene は System を一切知らない。
// System の登録・Update は GameManager が一元管理する。
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

    // シリアライズ
    void Serialize(nlohmann::json& out) const;
    nlohmann::json SerializeGameObject(std::shared_ptr<GameObject> obj) const;
    void Deserialize(const nlohmann::json& in);
    void DeserializeGameObject(const nlohmann::json& oj, std::shared_ptr<class GameObject> parent);

    // Entity → GameObject のマッピング登録
    void RegisterGameObject(Entity e, std::shared_ptr<GameObject> obj) {
        m_entityToObject[e] = obj;
    }

    // 生成・破棄
    std::shared_ptr<GameObject> CreateGameObject(const std::string& name = "GameObject");
    std::shared_ptr<GameObject> Instantiate(const std::string& filepath, const Math::Vector3& position = Math::Vector3::Zero);
    void Destroy(std::shared_ptr<GameObject> obj);

    // 内部リスト操作
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
        // リーク修正: Entity -> shared_ptr のマッピングも必ず消す
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
