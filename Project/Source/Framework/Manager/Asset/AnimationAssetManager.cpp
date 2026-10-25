#include "../../../Pch.h"
#include "AnimationAssetManager.h"

// Forward declaration of dummy AnimationClip for now.


AnimationAssetManager::~AnimationAssetManager() = default;

AssetHandle<AnimationClip> AnimationAssetManager::CreateAnimation(const AssetAnimationData& data)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto spAnim = std::make_unique<AnimationClip>();
	spAnim->name = data.name;

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
	slot.resource = std::move(spAnim);
	slot.generation++;
	slot.active = true;

	AssetHandle<AnimationClip> handle(slotIndex, slot.generation);
	if (!data.name.empty()) {
		m_cache[data.name] = handle;
	}

	return handle;
}

AnimationClip* AnimationAssetManager::Get(AssetHandle<AnimationClip> handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!handle.IsValid() || handle.index >= m_slots.size()) return nullptr;
	
	auto& slot = m_slots[handle.index];
	if (slot.active && slot.generation == handle.generation) {
		return slot.resource.get();
	}
	return nullptr;
}

void AnimationAssetManager::Release(AssetHandle<AnimationClip> handle)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!handle.IsValid() || handle.index >= m_slots.size()) return;

	auto& slot = m_slots[handle.index];
	if (slot.active && slot.generation == handle.generation) {
		slot.resource.reset();
		slot.active = false;
	}
}

