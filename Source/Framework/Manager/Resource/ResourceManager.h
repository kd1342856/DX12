#pragma once
#include "../../JobSystem/JobSystem.h"

class ResourceManager {
public:
    static ResourceManager& Instance();

    // Load model asynchronously. Returns a shared pointer that will be populated once loaded.
    // If already loaded or loading, returns the existing pointer.
    std::shared_ptr<ModelData> LoadModelAsync(const std::string& filepath);
    
    // Get a model if it's already loaded or in progress. Returns nullptr if not known.
    std::shared_ptr<ModelData> GetModel(const std::string& filepath);

    // Texture functions
    std::shared_ptr<Texture> LoadTextureAsync(const std::string& filepath);
    std::shared_ptr<Texture> GetTexture(const std::string& filepath);

    // Clear all cached resources
    void Clear();

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    std::unordered_map<std::string, std::shared_ptr<ModelData>> m_modelCache;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    std::mutex m_mutex;
};
