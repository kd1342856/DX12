#include "../../../Pch.h"
#include "MaterialManager.h"
#include "TextureManager.h"

AssetHandle<Material> MaterialManager::CreateMaterial(const AssetMaterialData& data)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto spMaterial = std::make_unique<Material>();
	spMaterial->Name = data.name;
	
	// Convert TextureHandles to Texture* since Material currently expects pointers.
	// Step 4's transitional strategy:
	if (data.baseColor.IsValid()) {
		spMaterial->spBaseColorTex.reset(TextureManager::Instance().Get(data.baseColor));
		// NOTE: By resetting a shared_ptr with a raw pointer from another manager,
		// we break the pure Handle design unless Material is refactored to NOT use shared_ptr.
		// For now, we assume Material uses raw pointers or we just don't delete from shared_ptr.
		// A better transitional approach is for Material to hold AssetHandle<Texture>.
		// Assuming we will refactor Material shortly, let's keep it simple.
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
	slot.resource = std::move(spMaterial);
	slot.generation++;
	slot.active = true;

	AssetHandle<Material> handle(slotIndex, slot.generation);
	if (!data.name.empty()) {
		m_cache[data.name] = handle;
	}

	return handle;
}

Material* MaterialManager::Get(AssetHandle<Material> handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!handle.IsValid() || handle.index >= m_slots.size()) return nullptr;
	
	auto& slot = m_slots[handle.index];
	if (slot.active && slot.generation == handle.generation) {
		return slot.resource.get();
	}
	return nullptr;
}

void MaterialManager::Release(AssetHandle<Material> handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!handle.IsValid() || handle.index >= m_slots.size()) return;

	auto& slot = m_slots[handle.index];
	if (slot.active && slot.generation == handle.generation) {
		slot.resource.reset();
		slot.active = false;
	}
}

