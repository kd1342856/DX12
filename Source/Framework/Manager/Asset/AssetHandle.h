#pragma once
#include <cstdint>
#include <limits>

template<class T>
class AssetHandle
{
public:
	static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	uint32_t index = INVALID_INDEX;
	uint32_t generation = 0;

	AssetHandle() = default;
	AssetHandle(uint32_t id, uint32_t gen) : index(id), generation(gen) {}

	bool IsValid() const {
		return index != INVALID_INDEX;
	}

	bool operator==(const AssetHandle<T>& other) const {
		return index == other.index && generation == other.generation;
	}

	bool operator!=(const AssetHandle<T>& other) const {
		return !(*this == other);
	}
};
