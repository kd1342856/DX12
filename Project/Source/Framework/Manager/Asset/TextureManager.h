#pragma once
#include <mutex>
#include "AssetHandle.h"
#include "AssetData.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "../../../Graphics/GPUResource/Texture/Texture.h"

class TextureManager
{
public:
	static TextureManager& Instance() { static TextureManager inst; return inst; }

	// colorSpaceはマテリアルのスロットで決める(BaseColor・EmissiveはSrgb、それ以外はLinear)。
	// 同じファイルでも色空間が違えば別のテクスチャとして読み込む。ミップは全段を生成する
	AssetHandle<Texture> LoadTexture(const std::string& filepath, TextureColorSpace colorSpace);
	AssetHandle<Texture> LoadTextureFromMemory(const std::string& key, const void* pData, size_t size, TextureColorSpace colorSpace);
	AssetHandle<Texture> CreateTexture(const AssetTextureData& data);
	// ミップを作らず、BC7にも圧縮せず、値をそのまま(Linear)読む。リファレンス比較の見本など、画素の値をそのまま比べたいとき用
	AssetHandle<Texture> LoadTextureExact(const std::string& cacheKey, const std::string& filepath);

	// Handle -> Texture* (for public access with generation check)
	Texture* Get(AssetHandle<Texture> handle);

	// Handle -> Texture* without lock (called from GPUUploadQueue::Process on main thread)
	Texture* GetRaw(AssetHandle<Texture> handle);

	// 読み込み中(LoadTextureの実行中)のテクスチャを解放しないこと。デコードはロックの外で行っている
	void Release(AssetHandle<Texture> handle);

private:
	// スロットを確保し、ロックの外でdecodeを呼んでCPU側のデータを作り、GPUへのアップロードを予約する
	AssetHandle<Texture> LoadWithDecoder(const std::string& cacheKey, const std::function<bool(Texture&, TextureUploadData&)>& decode);

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

