#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"

class ScriptComponent : public ComponentBase {
public:
    virtual void Awake() override {}
    virtual void Start() override {}
    virtual void Update() override {}
    virtual void PostUpdate() override {}

    virtual void OnCollisionEnter(class GameObject* other) {}
    virtual void OnCollisionStay(class GameObject* other) {}
    virtual void OnCollisionExit(class GameObject* other) {}
    virtual void OnTriggerEnter(class GameObject* other) {}
    virtual void OnTriggerStay(class GameObject* other) {}
    virtual void OnTriggerExit(class GameObject* other) {}
    virtual void PreDraw() override {}
    virtual void Draw() override {}
    virtual void Serialize(nlohmann::json& out) const override {}
    virtual void Deserialize(const nlohmann::json& in) override {}
    virtual void ImGuiUpdate() override {}
};

REGISTER_COMPONENT(ScriptComponent);

