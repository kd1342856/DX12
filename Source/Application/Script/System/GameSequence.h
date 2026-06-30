#pragma once

class GameSequence : public NativeScript {
public:
    enum class State {
        Playing,
        GameClear
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

    void NotifyExorcised();

    static GameSequence* GetInstance() { return s_instance; }

private:
    static GameSequence* s_instance;
    State m_currentState = State::Playing;
    float m_clearTimer = 0.0f;
};
