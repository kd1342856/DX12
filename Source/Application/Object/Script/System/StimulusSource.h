#pragma once

// ゴーストの注意を引く刺激(光・音)の発生源。
// ランプ/ラジカセ等、マップに置かれたオブジェクトに付ける。GhostAIは自分の種類に対応する
// 刺激だけを感知し(影女=光、家鳴=音)、有効な発生源へ誘導される(GhostAI::State::Lured)。
// 同じエンティティ(またはその子)にPointLightData/SpotLightDataがあれば、有効/無効をライトの点灯状態に同期する。
class LightSwitch;

enum class StimulusType
{
    Light,
    Sound,
};

class StimulusSource : public NativeScript
{
public:
    void Start() override;
    void Update(float deltaTime) override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;

    bool IsActive() const
    {
        return m_isActive;
    }
    void SetActive(bool active);
    void Toggle()
    {
        SetActive(!m_isActive);
    }
    // 切り替えた後に呼ぶ。壁スイッチに接続されていればそのスイッチ音、そうでなければ紐スイッチの点灯/消灯音を鳴らす
    void PlayToggleSound() const;
    // 点灯中のライトを指定時間チカチカさせる(点灯状態m_isActive自体は変えない)。ゴースト接近時の演出用。
    void Flicker(float duration);
    bool IsFlickering() const
    {
        return m_flickerTimer > 0.0f;
    }

    Math::Vector3 GetWorldPosition() const;

    // 指定した種類で有効な発生源のうち、posから自身のm_radius以内にある最も近いものを返す(なければnullptr)。
    static StimulusSource* FindNearestActive(StimulusType type, const Math::Vector3& pos);

    StimulusType m_type = StimulusType::Light;
    bool m_isActive = false;
    float m_radius = 12.0f;               // ゴーストがこの距離以内にいれば感知する
    bool m_interactable = true;           // プレイヤーがFキーでon/offできるか
    bool m_isRoomLight = true;            // 部屋の電気(天井照明等)か。ランプ等の小さな明かりはfalse。影女の退治条件はこれだけを数える
    bool m_controlledBySwitch = false;    // 実行時のみ。LightSwitchに接続されている(本体ではなく壁スイッチで操作する)
    LightSwitch* m_ownerSwitch = nullptr; // 実行時のみ。接続しているLightSwitch
    float m_soundVolume = 0.8f;           // 紐スイッチ(本体を直接操作した時)の音量

private:
    void ApplyToLights();
    void SetLightsEnabled(bool enabled);

    float m_flickerTimer = 0.0f;       // 残りの点滅時間
    float m_flickerToggleTimer = 0.0f; // 次に点/消を切り替えるまでの時間
    bool m_flickerLit = true;          // 点滅中の現在の見た目(点いているか)
};
