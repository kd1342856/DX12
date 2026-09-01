#include "../../../Pch.h"
#include "TextureManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Graphics/GPUUploadQueue/GPUUploadQueue.h"

// LoadTexture
// Can be called from a worker thread.
// Only CPU loading (WIC decode) is performed here.
// GPU upload is deferred to main thread via GPUUploadQueue.
AssetHandle<Texture> TextureManager::LoadTexture(const std::string& filepath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Cache check
    auto it = m_cache.find(filepath);
    if (it != m_cache.end()) {
        if (GetRaw(it->second)) {
            return it->second;
        }
    }

    // Allocate slot (GPU resource not created yet)
    auto spTexture = std::make_unique<Texture>();

    uint32_t slotIndex = m_nextIndex;
    for (uint32_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].active) { slotIndex = i; break; }
    }
    if (slotIndex == (uint32_t)m_slots.size()) {
        m_slots.push_back({});
        m_nextIndex++;
    }

    auto& slot = m_slots[slotIndex];
    slot.generation++;
    slot.active = true;

    AssetHandle<Texture> handle(slotIndex, slot.generation);
    m_cache[filepath] = handle;

    // CPU load only (worker thread safe)
    TextureUploadData cpuData;
    if (!spTexture->LoadCPU(filepath, cpuData)) {
        slot.active = false;
        return AssetHandle<Texture>();
    }

    // Store the texture object now (needed for GetRaw in Process())
    slot.resource = std::move(spTexture);

    // Defer GPU upload to main thread via GPUUploadQueue (Handle only, no shared_ptr)
    cpuData.targetHandle = handle;
    UploadRequest req;
    req.data = std::move(cpuData);
    GPUUploadQueue::Instance().Submit(std::move(req));

    return handle;
}

AssetHandle<Texture> TextureManager::LoadTextureFromMemory(const std::string& key, const void* pData, size_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Cache check
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        if (GetRaw(it->second)) {
            return it->second;
        }
    }

    // Allocate slot
    auto spTexture = std::make_unique<Texture>();

    uint32_t slotIndex = m_nextIndex;
    for (uint32_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].active) { slotIndex = i; break; }
    }
    if (slotIndex == (uint32_t)m_slots.size()) {
        m_slots.push_back({});
        m_nextIndex++;
    }

    auto& slot = m_slots[slotIndex];
    slot.generation++;
    slot.active = true;

    AssetHandle<Texture> handle(slotIndex, slot.generation);
    m_cache[key] = handle;

    // CPU load only
    TextureUploadData cpuData;
    if (!spTexture->LoadCPUFromMemory(pData, size, cpuData)) {
        slot.active = false;
        return AssetHandle<Texture>();
    }

    slot.resource = std::move(spTexture);

    cpuData.targetHandle = handle;
    UploadRequest req;
    req.data = std::move(cpuData);
    GPUUploadQueue::Instance().Submit(std::move(req));

    return handle;
}

AssetHandle<Texture> TextureManager::CreateTexture(const AssetTextureData& data)
{
    // TODO: Implement embedded texture support
    return AssetHandle<Texture>();
}

Texture* TextureManager::Get(AssetHandle<Texture> handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!handle.IsValid() || handle.index >= m_slots.size()) return nullptr;
    auto& slot = m_slots[handle.index];
    if (slot.active && slot.generation == handle.generation) {
        return slot.resource.get();
    }
    return nullptr;
}

// GetRaw - No lock version for main thread use (GPUUploadQueue::Process)
Texture* TextureManager::GetRaw(AssetHandle<Texture> handle)
{
    if (!handle.IsValid() || handle.index >= m_slots.size()) return nullptr;
    auto& slot = m_slots[handle.index];
    if (slot.active && slot.generation == handle.generation) {
        return slot.resource.get();
    }
    return nullptr;
}

void TextureManager::Release(AssetHandle<Texture> handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!handle.IsValid() || handle.index >= m_slots.size()) return;
    auto& slot = m_slots[handle.index];
    if (slot.active && slot.generation == handle.generation) {
        slot.resource.reset();
        slot.active = false;
    }
}