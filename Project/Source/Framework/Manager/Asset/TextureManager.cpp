#include "../../../Pch.h"
#include "TextureManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"
#include "../../../Graphics/GPUUploadQueue/GPUUploadQueue.h"

namespace
{
    // 同じファイルでも色空間が違えば中身(フォーマット)が違うので、キャッシュのキーに含める
    std::string MakeCacheKey(const std::string& path, TextureColorSpace colorSpace)
    {
        return path + (colorSpace == TextureColorSpace::Srgb ? "|sRGB" : "|Linear");
    }
}

// LoadTexture
// Can be called from a worker thread.
// Only CPU loading (WIC decode) is performed here.
// GPU upload is deferred to main thread via GPUUploadQueue.
AssetHandle<Texture> TextureManager::LoadTexture(const std::string& filepath, TextureColorSpace colorSpace)
{
    return LoadWithDecoder(MakeCacheKey(filepath, colorSpace), [&](Texture& texture, TextureUploadData& outData)
    {
        return texture.LoadCPU(filepath, outData, colorSpace, true);
    });
}

AssetHandle<Texture> TextureManager::LoadTextureFromMemory(const std::string& key, const void* pData, size_t size, TextureColorSpace colorSpace)
{
    return LoadWithDecoder(MakeCacheKey(key, colorSpace), [&](Texture& texture, TextureUploadData& outData)
    {
        return texture.LoadCPUFromMemory(pData, size, outData, colorSpace, true);
    });
}

AssetHandle<Texture> TextureManager::LoadWithDecoder(const std::string& cacheKey, const std::function<bool(Texture&, TextureUploadData&)>& decode)
{
    Texture* pTexture = nullptr;
    AssetHandle<Texture> handle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Cache check
        auto it = m_cache.find(cacheKey);
        if (it != m_cache.end()) {
            if (GetRaw(it->second)) {
                return it->second;
            }
        }

        // Allocate slot (GPU resource not created yet)
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
        // 先にTextureを置いておく。状態はLoadingCPUなので、読み込みが終わるまで描画には使われず、
        // 同じファイルを別のスレッドが要求しても二重には読み込まない
        slot.resource = std::make_unique<Texture>();
        pTexture = slot.resource.get();

        handle = AssetHandle<Texture>(slotIndex, slot.generation);
        m_cache[cacheKey] = handle;
    }

    // デコードとミップの生成は重いので、ロックの外で行う(複数のワーカースレッドで並列に読み込めるように)
    TextureUploadData cpuData;
    if (!decode(*pTexture, cpuData))
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& slot = m_slots[handle.index];
        if (slot.active && slot.generation == handle.generation) {
            slot.resource.reset();
            slot.active = false;
        }
        auto it = m_cache.find(cacheKey);
        if (it != m_cache.end() && it->second.index == handle.index && it->second.generation == handle.generation) {
            m_cache.erase(it);
        }
        return AssetHandle<Texture>();
    }

    // Defer GPU upload to main thread via GPUUploadQueue (Handle only, no shared_ptr)
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