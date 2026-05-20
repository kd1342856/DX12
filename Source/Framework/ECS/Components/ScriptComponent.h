#pragma once
#include "../../DirectX/Utility/ClassAssembly.h"
#include "../ComponentBase.h"

class ScriptComponent : public ComponentBase {
public:
    virtual void Awake() {}
    virtual void Start() {}
    virtual void Update() {}
    virtual void ImGuiUpdate() {}
};

REGISTER_COMPONENT(ScriptComponent);
