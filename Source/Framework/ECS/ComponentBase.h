#pragma once
#include "../Library/nlohmann/json.hpp"
#include "../Object/Object.h"

class GameObject;

// =============================================
// ComponentBase
// ?S?R???|?[?l???g????N???X
// =============================================
class ComponentBase : public Object {
public:
    ComponentBase() {}
    virtual ~ComponentBase() {}

    virtual const char* GetComponentName() const { return "Component"; }

    // ???C?t?T?C?N??
    virtual void Awake() {}
    virtual void Start() {}
    virtual void Update() {}
    virtual void PostUpdate() {}
    virtual void PreDraw() {}
    virtual void Draw() {}
    virtual void ImGuiUpdate() {}

    // ?V???A???C?Y
    virtual void Serialize(nlohmann::json& out) const {}
    virtual void Deserialize(const nlohmann::json& in) {}

    // ECS?f?[?^?o?^?iDataType?????R???|?[?l???g???I?[?o?[???C?h????j
    // AddComponent(shared_ptr<ComponentBase>) ?o?R?????????
    virtual void RegisterECSData() {}

    void SetGameObject(GameObject* owner) { m_owner = owner; }
    GameObject* GetGameObject() const { return m_owner; }

protected:
    GameObject* m_owner = nullptr;
};

