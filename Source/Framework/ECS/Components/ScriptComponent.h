#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"

class ScriptComponent : public ComponentBase {
public:
    virtual void Awake() override {}
    virtual void Start() override {}
    virtual void Update() override {}
    virtual void PostUpdate() override {}
    virtual void PreDraw() override {}
    virtual void Draw() override {}
    virtual void Serialize(nlohmann::json& out) const override {}
    virtual void Deserialize(const nlohmann::json& in) override {}
    virtual void ImGuiUpdate() override {}
};

REGISTER_COMPONENT(ScriptComponent);

