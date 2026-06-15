#include "ResourceManager.h"
#include "../../Graphics/Geometry/Model/ModelLoader.h"
#include "../DirectX/Utility/Logger.h"

std::shared_ptr<ModelData> ResourceManager::LoadModelAsync(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if already loaded or loading
    auto it = m_modelCache.find(filepath);
    if (it != m_modelCache.end()) {
        return it->second;
    }

    // Create an empty ModelData
    auto pModelData = std::make_shared<ModelData>();
    m_modelCache[filepath] = pModelData;

    // Queue the load job
    JobSystem::Instance().Execute([filepath, pModelData]() {
        Modeloader loader;
        if (!loader.Load(filepath, pModelData.get())) {
            Logger::Instance().AddLog(Logger::LogLevel::Error, "Failed to async load model: " + filepath);
        } else {
            Logger::Instance().AddLog(Logger::LogLevel::Info, "Successfully async loaded model: " + filepath);
        }
    });

    return pModelData;
}

std::shared_ptr<ModelData> ResourceManager::GetModel(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modelCache.find(filepath);
    if (it != m_modelCache.end()) {
        return it->second;
    }
    return nullptr;
}

void ResourceManager::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_modelCache.clear();
}