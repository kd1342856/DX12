#pragma once
#include <Audio.h>
#include <codecvt>
#include <locale>

class AudioManager
{
public:
    static AudioManager& Instance();

    void Init();
    void Update();
    void Shutdown();

    // SE(効果音)の再生。vol=音量(0.0~1.0), pitch=ピッチ(-1.0~1.0), pan=パン(-1.0~1.0)
    void PlaySE(const std::string& filepath, float vol = 1.0f, float pitch = 0.0f, float pan = 0.0f);

    // ワールド座標で鳴らすSE。リスナー(SetListener)からの距離で減衰し、左右にパンする。maxDist以上離れていれば鳴らさない
    void PlaySEAt(const std::string& filepath, const Math::Vector3& pos, float vol = 1.0f, float maxDist = 15.0f);

    // 聞き手(プレイヤー)の位置と向き。PlaySEAtの減衰・パンに使う
    void SetListener(const Math::Vector3& pos, const Math::Vector3& forward)
    {
        m_listenerPos = pos;
        m_listenerForward = forward;
    }

    // ループ再生や途中停止が必要なSE用のインスタンスを作る(足音など)。呼び出し側が所有し、Shutdownより前に破棄すること
    std::unique_ptr<DirectX::SoundEffectInstance> CreateSEInstance(const std::string& filepath);

    // BGMの再生。vol=音量(0.0~1.0)
    void PlayBGM(const std::string& filepath, float vol = 1.0f);
    void StopBGM();
    void SetBGMVolume(float vol);
    void PauseBGM();
    void ResumeBGM();
    const std::string& GetCurrentBGM() const
    {
        return m_currentBGM;
    }
    DirectX::SoundState GetBGMState() const;

    // --- 音量バス(Sound Managerウィンドウで調整)。各再生時の音量にさらに掛け合わされる ---
    float GetMasterVolume() const
    {
        return m_masterVolume;
    }
    float GetBGMBusVolume() const
    {
        return m_bgmBusVolume;
    }
    // CreateSEInstanceで作ったインスタンスにSetVolumeする時は、この値を掛けること
    float GetSEVolume() const
    {
        return m_seBusVolume;
    }
    bool IsMuted() const
    {
        return m_muted;
    }
    void SetMasterVolume(float vol);
    void SetBGMBusVolume(float vol);
    void SetSEVolume(float vol)
    {
        m_seBusVolume = vol;
    }
    void SetMuted(bool muted);

    // --- 曲ごとの音量(Sound Managerで調整・保存)。BGMの最終音量 = PlayBGMのvol × 曲の音量 × BGMバス ---
    // 曲を使う側(TitleScene等)が表示名と初期音量を登録する。保存済みの値があればそちらを優先する
    void RegisterBGMTrack(const std::string& filepath, const std::string& label, float defaultVolume);
    float GetBGMTrackVolume(const std::string& filepath) const;
    void SetBGMTrackVolume(const std::string& filepath, float vol);
    // 表示名(未登録なら空)
    const std::string& GetBGMTrackLabel(const std::string& filepath) const;
    // 音量が設定/登録されている曲の一覧
    std::vector<std::string> GetBGMTrackPaths() const;

    // 音量設定の保存/読み込み(Initで自動的に読み込む)
    static constexpr const char* kSettingsPath = "Asset/Data/AudioSettings.json";
    void LoadSettings();
    void SaveSettings() const;

    // 読み込み済み(キャッシュ済み)のサウンドのパス一覧
    std::vector<std::string> GetLoadedSoundPaths() const;

private:
    void ApplyVolumes();
    AudioManager() = default;
    ~AudioManager() = default;

    std::unique_ptr<DirectX::AudioEngine> m_audioEngine;
    std::unordered_map<std::string, std::unique_ptr<DirectX::SoundEffect>> m_soundEffects;

    std::unique_ptr<DirectX::SoundEffectInstance> m_bgmInstance;
    std::string m_currentBGM;
    float m_bgmBaseVolume = 1.0f;                        // PlayBGM/SetBGMVolumeで指定された音量(バス適用前)
    std::map<std::string, float> m_bgmTrackVolumes;      // 曲ごとの音量(保存対象)
    std::map<std::string, std::string> m_bgmTrackLabels; // 曲の表示名(実行時のみ)

    float m_masterVolume = 1.0f;
    float m_bgmBusVolume = 1.0f;
    float m_seBusVolume = 1.0f;
    bool m_muted = false;

    Math::Vector3 m_listenerPos = Math::Vector3::Zero;
    Math::Vector3 m_listenerForward = Math::Vector3(0, 0, 1);

    DirectX::SoundEffect* GetOrLoadSound(const std::string& filepath);
    std::wstring StringToWString(const std::string& str);
};
