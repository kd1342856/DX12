#pragma once
#include "../../../../Framework/ECS/Components/Data/NativeScript.h"

class Player : public NativeScript {
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

    float m_moveSpeed = 10.0f;
    bool m_useGravity = true;
    float m_gravityStrength = 9.8f;
    float m_velocityY = 0.0f;
    bool m_isGrounded = false;

    void OnCollisionEnter(GameObject* other) override;
    void OnCollisionStay(GameObject* other) override;
};

