#pragma once
#include <mutex>
#include "AssetHandle.h"
#include "AssetData.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include "../../../Graphics/GPUResource/Texture/Texture.h"

class TextureManager
{
public:
	static TextureManager& Instance() { static TextureManager inst; return inst; }

	AssetHandle<Texture> LoadTexture(const std::string& filepath);
	AssetHandle<Texture> CreateTexture(const AssetTextureData& data);
	
	Texture* Get(AssetHandle<Texture> handle);
	void Release(AssetHandle<Texture> handle);

private:
	struct TextureSlot {
		std::unique_ptr<Texture> resource;
		uint32_t generation = 0;
		bool active = false;
	};

	std::vector<TextureSlot> m_slots;
	std::unordered_map<std::string, AssetHandle<Texture>> m_cache;
	uint32_t m_nextIndex = 0;
	std::mutex m_mutex;
};

