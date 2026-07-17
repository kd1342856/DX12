#include "../../../Pch.h"
#include "../Asset/AssetManager.h"

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
        LoadModelOption option;
        if (!AssetManager::Instance().LoadModel(filepath, option, pModelData.get())) {
            Logger::Instance().AddLog(Logger::LogLevel::Error, "Failed to async load model: " + filepath);
        } else {
            pModelData->SetLoaded(true);
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
    m_textureCache.clear();
}

std::shared_ptr<Texture> ResourceManager::LoadTextureAsync(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textureCache.find(filepath);
    if (it != m_textureCache.end()) {
        return it->second;
    }

    auto pTexture = std::make_shared<Texture>();
    m_textureCache[filepath] = pTexture;

    JobSystem::Instance().Execute([filepath, pTexture]() {
        if (!pTexture->Load(&GraphicsDevice::Instance(), filepath)) {
            Logger::Instance().AddLog(Logger::LogLevel::Error, "Failed to async load texture: " + filepath);
        } else {
            Logger::Instance().AddLog(Logger::LogLevel::Info, "Successfully async loaded texture: " + filepath);
        }
    });

    return pTexture;
}

std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_textureCache.find(filepath);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    return nullptr;
}

ResourceManager& ResourceManager::Instance()
{
    static ResourceManager instance;
    return instance;
}
