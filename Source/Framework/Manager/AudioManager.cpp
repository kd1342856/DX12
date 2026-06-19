#include "AudioManager.h"
#include "../DirectX/Utility/Logger.h"
#include <Windows.h>

void AudioManager::Init() {
    DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_Default;
#ifdef _DEBUG
    eflags |= DirectX::AudioEngine_Debug;
#endif

    try {
        m_audioEngine = std::make_unique<DirectX::AudioEngine>(eflags);
        Logger::Instance().AddLog(Logger::LogLevel::Info, "AudioManager: Initialized AudioEngine successfully.");
    }
    catch (...) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "AudioManager: Failed to initialize AudioEngine.");
    }
}

void AudioManager::Update() {
    if (m_audioEngine) {
        if (!m_audioEngine->Update()) {
            // Audio device is lost (e.g., unplugged headphones)
            if (m_audioEngine->IsCriticalError()) {
                Logger::Instance().AddLog(Logger::LogLevel::Warning, "AudioManager: Critical Error detected.");
            }
        }
    }
}

void AudioManager::Shutdown() {
    if (m_audioEngine) {
        m_audioEngine->Suspend();
    }
    m_bgmInstance.reset();
    m_soundEffects.clear();
    m_audioEngine.reset();
}

std::wstring AudioManager::StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

DirectX::SoundEffect* AudioManager::GetOrLoadSound(const std::string& filepath) {
    if (!m_audioEngine) return nullptr;

    auto it = m_soundEffects.find(filepath);
    if (it != m_soundEffects.end()) {
        return it->second.get();
    }

    try {
        std::wstring wpath = StringToWString(filepath);
        auto effect = std::make_unique<DirectX::SoundEffect>(m_audioEngine.get(), wpath.c_str());
        DirectX::SoundEffect* pEffect = effect.get();
        m_soundEffects[filepath] = std::move(effect);
        return pEffect;
    }
    catch (...) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "AudioManager: Failed to load sound: " + filepath);
        return nullptr;
    }
}

void AudioManager::PlaySE(const std::string& filepath, float vol, float pitch, float pan) {
    DirectX::SoundEffect* effect = GetOrLoadSound(filepath);
    if (effect) {
        effect->Play(vol, pitch, pan);
    }
}

void AudioManager::PlayBGM(const std::string& filepath, float vol) {
    if (m_currentBGM == filepath && m_bgmInstance && m_bgmInstance->GetState() == DirectX::SoundState::PLAYING) {
        return; // Already playing
    }

    DirectX::SoundEffect* effect = GetOrLoadSound(filepath);
    if (effect) {
        m_bgmInstance = effect->CreateInstance();
        if (m_bgmInstance) {
            m_bgmInstance->SetVolume(vol);
            m_bgmInstance->Play(true); // Loop = true
            m_currentBGM = filepath;
            Logger::Instance().AddLog(Logger::LogLevel::Info, "AudioManager: Playing BGM: " + filepath);
        }
    }
}

void AudioManager::StopBGM() {
    if (m_bgmInstance) {
        m_bgmInstance->Stop();
        m_currentBGM.clear();
    }
}

void AudioManager::SetBGMVolume(float vol) {
    if (m_bgmInstance) {
        m_bgmInstance->SetVolume(vol);
    }
}

AudioManager& AudioManager::Instance()
{
    static AudioManager instance;
    return instance;
}
