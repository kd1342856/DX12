#include "../../../Pch.h"
#include "TextureManager.h"
#include "../../../Graphics/Device/GraphicsDevice.h"

AssetHandle<Texture> TextureManager::LoadTexture(const std::string& filepath)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	// Check cache
	auto it = m_cache.find(filepath);
	if (it != m_cache.end()) {
		if (Get(it->second)) {
			return it->second;
		}
	}

	auto spTexture = std::make_unique<Texture>();
	if (!spTexture->Load(&GraphicsDevice::Instance(), filepath)) {
		return AssetHandle<Texture>();
	}

	// Find an empty slot
	uint32_t slotIndex = m_nextIndex;
	for (uint32_t i = 0; i < m_slots.size(); ++i) {
		if (!m_slots[i].active) {
			slotIndex = i;
			break;
		}
	}

	if (slotIndex == m_slots.size()) {
		m_slots.push_back({});
		m_nextIndex++;
	}

	auto& slot = m_slots[slotIndex];
	slot.resource = std::move(spTexture);
	slot.generation++;
	slot.active = true;

	AssetHandle<Texture> handle(slotIndex, slot.generation);
	m_cache[filepath] = handle;

	return handle;
}

AssetHandle<Texture> TextureManager::CreateTexture(const AssetTextureData& data)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	// NOTE: In Step 4, we might support creating textures from raw pixel data in memory (for embedded glTF textures).
	// But since Texture class currently doesn't have an easy CreateFromMemory function, we leave it as a TODO or implement a simple wrapper.
	// For now, assume LoadTexture is the primary path.
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

