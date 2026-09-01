#include "../../../Pch.h"
#include "MaterialManager.h"
#include "TextureManager.h"

AssetHandle<Material> MaterialManager::CreateMaterial(const AssetMaterialData& data)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto spMaterial = std::make_unique<Material>();
	spMaterial->Name = data.name;
	spMaterial->BaseColor = data.baseColor;
	spMaterial->Normal = data.normal;
	spMaterial->MetallicRoughness = data.metallicRoughness;
	spMaterial->Occlusion = data.occlusion;
	spMaterial->Emissive = data.emissive;

	spMaterial->Constants.baseColorFactor = data.baseColorFactor;
	spMaterial->Constants.metallicFactor = data.metallicFactor;
	spMaterial->Constants.roughnessFactor = data.roughnessFactor;
	spMaterial->Constants.normalScale = data.normalScale;
	spMaterial->Constants.occlusionStrength = data.occlusionStrength;
	spMaterial->Constants.emissiveStrength = data.emissiveStrength;
	spMaterial->Constants.emissiveFactorX = data.emissiveFactorX;
	spMaterial->Constants.emissiveFactorY = data.emissiveFactorY;
	spMaterial->Constants.emissiveFactorZ = data.emissiveFactorZ;
	spMaterial->Constants.alphaMode = data.alphaMode;
	spMaterial->Constants.proceduralType = data.proceduralType;

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

