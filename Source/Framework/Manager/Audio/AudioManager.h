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

    // SE・亥柑譫憺浹・峨・蜀咲函縲Ｗol=髻ｳ驥・0.0~1.0), pitch=繝斐ャ繝・-1.0~1.0), pan=繝代Φ(-1.0~1.0)
    void PlaySE(const std::string& filepath, float vol = 1.0f, float pitch = 0.0f, float pan = 0.0f);
    
    // BGM縺ｮ蜀咲函縲Ｗol=髻ｳ驥・0.0~1.0)
    void PlayBGM(const std::string& filepath, float vol = 1.0f);
    void StopBGM();
    void SetBGMVolume(float vol);

private:
    AudioManager() = default;
    ~AudioManager() = default;

    std::unique_ptr<DirectX::AudioEngine> m_audioEngine;
    std::unordered_map<std::string, std::unique_ptr<DirectX::SoundEffect>> m_soundEffects;
    
    std::unique_ptr<DirectX::SoundEffectInstance> m_bgmInstance;
    std::string m_currentBGM;

    DirectX::SoundEffect* GetOrLoadSound(const std::string& filepath);
    std::wstring StringToWString(const std::string& str);
};

