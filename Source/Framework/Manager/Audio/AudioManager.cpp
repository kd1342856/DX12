#include "../../../Pch.h"
#include "AudioManager.h"

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
    LoadSettings();
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
        effect->Play(vol * m_seBusVolume, pitch, pan);
    }
}

void AudioManager::ComputeSpatial(const Math::Vector3& pos, float maxDist, float& outAtten, float& outPan) const {
    outAtten = 0.0f;
    outPan = 0.0f;
    Math::Vector3 toSound = pos - m_listenerPos;
    float dist = toSound.Length();
    if (dist >= maxDist) return;

    // Quadratic falloff sounds more natural than linear for indoor distances
    float atten = 1.0f - dist / maxDist;
    outAtten = atten * atten;

    // Pan by how far the sound is to the listener's right (horizontal plane only), kept partial
    // so a sound directly to one side never goes fully silent in the other ear
    Math::Vector3 flat(toSound.x, 0.0f, toSound.z);
    if (flat.LengthSquared() > 0.0001f) {
        flat.Normalize();
        Math::Vector3 right = Math::Vector3(0, 1, 0).Cross(m_listenerForward);
        right.y = 0.0f;
        if (right.LengthSquared() > 0.0001f) {
            right.Normalize();
            outPan = flat.Dot(right) * 0.7f;
        }
    }
}

void AudioManager::PlaySEAt(const std::string& filepath, const Math::Vector3& pos, float vol, float maxDist, float pitch) {
    float atten, pan;
    ComputeSpatial(pos, maxDist, atten, pan);
    if (atten <= 0.0f) return;
    PlaySE(filepath, vol * atten, pitch, pan);
}

std::unique_ptr<DirectX::SoundEffectInstance> AudioManager::CreateSEInstance(const std::string& filepath) {
    DirectX::SoundEffect* effect = GetOrLoadSound(filepath);
    if (!effect) return nullptr;
    return effect->CreateInstance();
}

float AudioManager::GetSoundDuration(const std::string& filepath) {
    DirectX::SoundEffect* effect = GetOrLoadSound(filepath);
    return effect ? effect->GetSampleDurationMS() / 1000.0f : 0.0f;
}

void AudioManager::PlayBGM(const std::string& filepath, float vol) {
    if (m_currentBGM == filepath && m_bgmInstance && m_bgmInstance->GetState() == DirectX::SoundState::PLAYING) {
        return; // Already playing
    }

    DirectX::SoundEffect* effect = GetOrLoadSound(filepath);
    if (effect) {
        m_bgmInstance = effect->CreateInstance();
        if (m_bgmInstance) {
            m_bgmBaseVolume = vol;
            m_currentBGM = filepath;
            ApplyVolumes();
            m_bgmInstance->Play(true); // Loop = true
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
    m_bgmBaseVolume = vol;
    ApplyVolumes();
}

void AudioManager::PauseBGM() {
    if (m_bgmInstance) m_bgmInstance->Pause();
}

void AudioManager::ResumeBGM() {
    if (m_bgmInstance) m_bgmInstance->Resume();
}

DirectX::SoundState AudioManager::GetBGMState() const {
    return m_bgmInstance ? m_bgmInstance->GetState() : DirectX::SoundState::STOPPED;
}

void AudioManager::SetMasterVolume(float vol) {
    m_masterVolume = vol;
    ApplyVolumes();
}

void AudioManager::SetBGMBusVolume(float vol) {
    m_bgmBusVolume = vol;
    ApplyVolumes();
}

void AudioManager::SetMuted(bool muted) {
    m_muted = muted;
    ApplyVolumes();
}

void AudioManager::SetPaused(bool paused) {
    if (!m_audioEngine) return;
    if (paused) m_audioEngine->Suspend();
    else        m_audioEngine->Resume();
}

void AudioManager::ApplyVolumes() {
    // Master/mute go through the engine's mastering voice so they also cover SE instances owned by
    // scripts (footsteps etc.); the BGM bus only touches the BGM instance.
    if (m_audioEngine) m_audioEngine->SetMasterVolume(m_muted ? 0.0f : m_masterVolume);
    if (m_bgmInstance) m_bgmInstance->SetVolume(m_bgmBaseVolume * GetBGMTrackVolume(m_currentBGM) * m_bgmBusVolume);
}

void AudioManager::RegisterBGMTrack(const std::string& filepath, const std::string& label, float defaultVolume) {
    m_bgmTrackLabels[filepath] = label;
    // keep a volume loaded from the settings file; only fall back to the caller's default
    m_bgmTrackVolumes.emplace(filepath, defaultVolume);
}

float AudioManager::GetBGMTrackVolume(const std::string& filepath) const {
    auto it = m_bgmTrackVolumes.find(filepath);
    return it != m_bgmTrackVolumes.end() ? it->second : 1.0f;
}

void AudioManager::SetBGMTrackVolume(const std::string& filepath, float vol) {
    m_bgmTrackVolumes[filepath] = vol;
    if (filepath == m_currentBGM) ApplyVolumes();
}

const std::string& AudioManager::GetBGMTrackLabel(const std::string& filepath) const {
    static const std::string kEmpty;
    auto it = m_bgmTrackLabels.find(filepath);
    return it != m_bgmTrackLabels.end() ? it->second : kEmpty;
}

std::vector<std::string> AudioManager::GetBGMTrackPaths() const {
    std::vector<std::string> paths;
    for (auto& [path, vol] : m_bgmTrackVolumes) paths.push_back(path);
    return paths;
}

void AudioManager::LoadSettings() {
    std::ifstream in(kSettingsPath);
    if (in.is_open()) {
        try {
            nlohmann::json j;
            in >> j;
            if (j.contains("master")) m_masterVolume = j["master"];
            if (j.contains("bgm"))    m_bgmBusVolume = j["bgm"];
            if (j.contains("se"))     m_seBusVolume = j["se"];
            if (j.contains("muted"))  m_muted = j["muted"];
            if (j.contains("bgmTracks") && j["bgmTracks"].is_object()) {
                for (auto& [path, vol] : j["bgmTracks"].items()) m_bgmTrackVolumes[path] = vol.get<float>();
            }
        }
        catch (...) {
            Logger::Instance().AddLog(Logger::LogLevel::Warning, "AudioManager: Failed to parse %s", kSettingsPath);
        }
    }
    ApplyVolumes();
}

void AudioManager::SaveSettings() const {
    nlohmann::json j;
    j["master"] = m_masterVolume;
    j["bgm"] = m_bgmBusVolume;
    j["se"] = m_seBusVolume;
    j["muted"] = m_muted;
    j["bgmTracks"] = nlohmann::json::object();
    for (auto& [path, vol] : m_bgmTrackVolumes) j["bgmTracks"][path] = vol;
    std::ofstream o(kSettingsPath);
    if (o.is_open()) {
        o << std::setw(4) << j << std::endl;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "AudioManager: Saved %s", kSettingsPath);
    }
}

std::vector<std::string> AudioManager::GetLoadedSoundPaths() const {
    std::vector<std::string> paths;
    for (auto& [path, effect] : m_soundEffects) paths.push_back(path);
    std::sort(paths.begin(), paths.end());
    return paths;
}

AudioManager& AudioManager::Instance()
{
    static AudioManager instance;
    return instance;
}
