#include "../../../Pch.h"
#include "PrefabManager.h"

nlohmann::json PrefabManager::GetPrefab(const std::string& filepath) {
    auto it = m_prefabCache.find(filepath);
    if (it != m_prefabCache.end()) {
        return it->second;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "PrefabManager: Failed to open prefab file: " + filepath);
        return nlohmann::json();
    }

    try {
        nlohmann::json j;
        file >> j;
        m_prefabCache[filepath] = j;
        Logger::Instance().AddLog(Logger::LogLevel::Info, "PrefabManager: Loaded prefab: " + filepath);
        return j;
    }
    catch (const nlohmann::json::exception& e) {
        Logger::Instance().AddLog(Logger::LogLevel::Error, "PrefabManager: JSON parse error in prefab: " + filepath + " -> " + e.what());
        return nlohmann::json();
    }
}

void PrefabManager::ClearCache() {
    m_prefabCache.clear();
}

PrefabManager& PrefabManager::Instance()
{
    static PrefabManager instance;
    return instance;
}
