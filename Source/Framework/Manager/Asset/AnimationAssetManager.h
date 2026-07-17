#pragma once
#include <mutex>
#include "AssetHandle.h"
#include "AssetData.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

class AnimationClip {
public:
	std::string name;
}; // Forward declaration

class AnimationAssetManager
{
public:
	static AnimationAssetManager& Instance() { static AnimationAssetManager inst; return inst; }

	~AnimationAssetManager();

	AssetHandle<AnimationClip> CreateAnimation(const AssetAnimationData& data);
	
	AnimationClip* Get(AssetHandle<AnimationClip> handle);
	void Release(AssetHandle<AnimationClip> handle);

private:
	struct AnimationSlot {
		std::unique_ptr<AnimationClip> resource;
		uint32_t generation = 0;
		bool active = false;
	};

	std::vector<AnimationSlot> m_slots;
	std::unordered_map<std::string, AssetHandle<AnimationClip>> m_cache;
	uint32_t m_nextIndex = 0;
	std::mutex m_mutex;
};

