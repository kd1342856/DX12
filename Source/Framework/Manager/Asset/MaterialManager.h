#pragma once
#include <mutex>
#include "AssetHandle.h"
#include "AssetData.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include "../../../Graphics/GPUResource/Material/Material.h"

class MaterialManager
{
public:
	static MaterialManager& Instance() { static MaterialManager inst; return inst; }

	AssetHandle<Material> CreateMaterial(const AssetMaterialData& data);
	
	Material* Get(AssetHandle<Material> handle);
	void Release(AssetHandle<Material> handle);

private:
	struct MaterialSlot {
		std::unique_ptr<Material> resource;
		uint32_t generation = 0;
		bool active = false;
	};

	std::vector<MaterialSlot> m_slots;
	std::unordered_map<std::string, AssetHandle<Material>> m_cache;
	uint32_t m_nextIndex = 0;
	std::mutex m_mutex;
};

