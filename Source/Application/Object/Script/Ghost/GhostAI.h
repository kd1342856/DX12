#pragma once

class GhostAI : public NativeScript {
public:
    enum class State
    {
        Idle,
        Wander,
        Hunt,
        Dead
    };

    void Awake() override;
    void Start() override;
    void Update() override;
    void PostUpdate() override;

    void PreDraw() override;
    void Draw() override;

    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;

    void ImGuiUpdate() override;

    void OnCollisionEnter(GameObject* other) override;
    void OnCollisionStay(GameObject* other) override;

    void Exorcise();
    void SetState(State state);

    bool IsExorcised() const { return m_isExorcised; }

private:
    float m_moveSpeed = 1.0f;
    float m_changeDirTimer = 0.0f;
    Math::Vector3 m_moveDir = { 0, 0, 0 };
    bool m_isExorcised = false;
    
    State m_currentState = State::Idle;
    
    int m_animIdle = 0;
    int m_animWander = 1;
    int m_animHunt = 2;
    int m_animDead = 3;
};
