#pragma once
#include "../Library/nlohmann/json.hpp"
#include "../DirectX/Utility/ClassAssembly.h"
#include "Object.h"
#include "../ECS/ComponentBase.h"
#include "../ECS/Entity/Entity.h"

class Scene;

// =============================================
// ECSデータ型の自動検出用トレイト
// ComponentにDataType定義があればECSへ自動登録
// =============================================
template<typename T, typename = void>
struct HasDataType : std::false_type {};

template<typename T>
struct HasDataType<T, std::void_t<typename T::DataType>> : std::true_type {};

// =============================================
// GameObject
// ECSのEntityを内包するフレームワーク層クラス
// ユーザーはECS EntityIDを意識しなくてよい
// =============================================
class GameObject : public Object, public std::enable_shared_from_this<GameObject> {
public:
    GameObject() {}

    // デストラクタでECS Entityを自動削除
    // シャットダウン時に GameManager が先に破棄されていた場合は安全にスキップ
    ~GameObject() {
        if (m_entityId != INVALID_ENTITY && GameManager::IsInstanceAlive()) {
            GameManager::Instance().GetECS().DestroyEntity(m_entityId);
            m_entityId = INVALID_ENTITY;
        }
    }

    void Start();
    void Update();
    void PostUpdate();
    void PreDraw();
    void Draw();
    void ImGuiUpdate();

    void Serialize(nlohmann::json& out) const;
    void Deserialize(const nlohmann::json& in);

    void SetScene(Scene* scene) { m_scene = scene; }
    Scene* GetScene() const { return m_scene; }

    void SetEntityID(Entity id) { m_entityId = id; }
    Entity GetEntityID() const { return m_entityId; }

    void SetParent(std::shared_ptr<GameObject> parent);

    void Destroy();

    GameObject* GetParent() const { return m_pParent; }
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_children; }

    // =============================================
    // AddComponent<T>
    // コンポーネントを追加し Awake() を呼ぶ
    // T::DataType が定義されていれば ECS に自動登録
    // =============================================
    template<class T, class... Args>
    std::shared_ptr<T> AddComponent(Args&&... args) {
        std::shared_ptr<T> pSp = std::make_shared<T>(std::forward<Args>(args)...);
        pSp->SetGameObject(this);
        m_components.push_back(pSp);
        // DataType があれば ECS に自動でデータ登録（if constexpr で分岐）
        if constexpr (HasDataType<T>::value) {
            typename T::DataType data{};
            GameManager::Instance().GetECS().AddComponent(m_entityId, data);
        }
        pSp->Awake();
        return pSp;
    }

    // デシリアライズ等で ComponentBase のポインタから追加する場合
    // RegisterECSData() を呼んでECS登録を行い、その後 Awake() を呼ぶ
    void AddComponent(std::shared_ptr<ComponentBase> comp) {
        if (comp) {
            comp->SetGameObject(this);
            m_components.push_back(comp);
            comp->RegisterECSData();
            comp->Awake();
        }
    }

    template<class T>
    std::shared_ptr<T> GetComponent() {
        for (auto& comp : m_components) {
            std::shared_ptr<T> target = std::dynamic_pointer_cast<T>(comp);
            if (target) { return target; }
        }
        return nullptr;
    }

public:
    const std::vector<std::shared_ptr<ComponentBase>>& GetComponentsList() const { return m_components; }

protected:
    bool m_isStarted = false;

private:
    std::vector<std::shared_ptr<ComponentBase>> m_components;
    std::vector<std::shared_ptr<GameObject>> m_children;
    GameObject* m_pParent = nullptr;
    Scene* m_scene = nullptr;
    Entity m_entityId = INVALID_ENTITY;
};

