#pragma once
#include "../Library/nlohmann/json.hpp"
#include "../Object/Object.h"

class GameObject;

// =============================================
// ComponentBase
// 全コンポーネントの基底クラス
// =============================================
class ComponentBase : public Object {
public:
    ComponentBase() {}
    virtual ~ComponentBase() {}

    virtual const char* GetComponentName() const { return "Component"; }

    // ライフサイクル
    virtual void Awake() {}
    virtual void Start() {}
    virtual void Update() {}
    virtual void ImGuiUpdate() {}

    // シリアライズ
    virtual void Serialize(nlohmann::json& out) const {}
    virtual void Deserialize(const nlohmann::json& in) {}

    // ECSデータ登録（DataTypeを持つコンポーネントがオーバーライドする）
    // AddComponent(shared_ptr<ComponentBase>) 経由の場合に呼ばれる
    virtual void RegisterECSData() {}

    void SetGameObject(GameObject* owner) { m_owner = owner; }
    GameObject* GetGameObject() const { return m_owner; }

protected:
    GameObject* m_owner = nullptr;
};