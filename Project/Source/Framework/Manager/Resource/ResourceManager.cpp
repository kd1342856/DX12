#include "../../../Pch.h"
#include "../Asset/AssetManager.h"
#include "../../../Graphics/GPUUploadQueue/GPUUploadQueue.h"

std::shared_ptr<ModelData> ResourceManager::LoadModelAsync(const std::string& filepath, bool mergeMeshes) {
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
    JobSystem::Instance().Execute([filepath, pModelData, mergeMeshes]() {
        LoadModelOption option;
        option.mergeMeshes = mergeMeshes;
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
        TextureUploadData data;
        if (!pTexture->LoadCPU(filepath, data)) {
            Logger::Instance().AddLog(Logger::LogLevel::Error, "Failed to async load texture: " + filepath);
        } else {
            std::function<void()> task = [pTexture, data = std::move(data), filepath]() {
                pTexture->CreateGPU(&GraphicsDevice::Instance(), data);
                pTexture->SetState(AssetState::Ready);
                Logger::Instance().AddLog(Logger::LogLevel::Info, "Successfully async loaded texture: " + filepath);
            };
            GPUUploadQueue::Instance().Submit(UploadRequest{ task });
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
