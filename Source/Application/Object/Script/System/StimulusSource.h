#pragma once

// ゴーストの注意を引く刺激(光・音)の発生源。
// ランプ/ラジカセ等、マップに置かれたオブジェクトに付ける。GhostAIは自分の種類に対応する
// 刺激だけを感知し(鬼火=光、家鳴=音)、有効な発生源へ誘導される(GhostAI::State::Lured)。
// 同じエンティティ(またはその子)にPointLightData/SpotLightDataがあれば、有効/無効をライトの点灯状態に同期する。
enum class StimulusType
{
    Light,
    Sound,
};

class StimulusSource : public NativeScript {
public:
    void Start() override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;

    bool IsActive() const { return m_isActive; }
    void SetActive(bool active);
    void Toggle() { SetActive(!m_isActive); }

    Math::Vector3 GetWorldPosition() const;

    // 指定した種類で有効な発生源のうち、posから自身のm_radius以内にある最も近いものを返す(なければnullptr)。
    static StimulusSource* FindNearestActive(StimulusType type, const Math::Vector3& pos);

    StimulusType m_type = StimulusType::Light;
    bool  m_isActive = false;
    float m_radius = 12.0f;       // ゴーストがこの距離以内にいれば感知する
    bool  m_interactable = true;  // プレイヤーがFキーでon/offできるか

private:
    void ApplyToLights();
};
