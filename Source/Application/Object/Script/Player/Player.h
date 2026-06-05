#pragma once
#include "../../../../Framework/ECS/Components/ScriptComponent.h"

class Player : public ScriptComponent {
public:
    void Awake() override;
    void Start() override;
    void Update() override;
    void PostUpdate() override;
    void PreDraw() override;
    void Draw() override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;
};

