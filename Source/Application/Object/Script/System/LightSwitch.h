#pragma once

class StimulusSource;

// 壁スイッチ。プレイヤーがFキーで押すと、名前で指定した照明(StimulusSourceを持つオブジェクト)を
// まとめてon/offする(例: 廊下の天井照明)。指定された照明は本体を直接Fキーで操作できなくなり、
// このスイッチ経由でのみ切り替えられる(ゴーストが勝手に点けるのは引き続き可能)。
// 点灯中はパイロットランプ(m_pilotMaterial)を光らせる。
class LightSwitch : public NativeScript {
public:
    void Start() override;
    void Update(float deltaTime) override;
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;

    void Toggle();
    bool IsOn() const;
    // スイッチ音(kabe_switch)をスイッチの位置から鳴らす
    void PlaySwitchSound() const;

    Math::Vector3 GetWorldPosition() const;

    std::string m_targetNames;                  // 操作する照明のGameObject名(カンマ区切りで複数可)
    std::string m_pilotMaterial = "Pilot_Lamp"; // 点灯中に光らせるマテリアル名
    bool m_interactable = true;
    float m_soundVolume = 0.8f; // スイッチ音の音量

private:
    void ResolveTargets();
    void UpdatePilotLamp();
    // レバー(ロッカー)をON/OFFの位置へ動かす。モデル側のSwitch_OFF_to_ON / Switch_ON_to_OFFを再生する。
    // 初回(読み込み直後)は再生せず終端のポーズへ直接合わせる。
    void UpdateRocker();

    std::vector<StimulusSource*> m_targets;
    bool m_resolved = false;
    int  m_rockerState = -1; // -1=未反映, 0=OFF, 1=ON
};
